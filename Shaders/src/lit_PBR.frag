#version 450
#define PI         3.14159265359
#define MAX_LIGHTS 256

// GPU side (GLSL)
struct Light {
    // Core properties (4 bytes + 1 byte + 3 padding bytes)
    int type;
    bool castShadows;

    // Position/direction properties (24 bytes, 16-byte aligned)
    vec3 position;
    vec3 direction;

    // Intensity and color properties (48 bytes, 16-byte aligned)
    vec3 Color;
    float intensity;

    // Shadow properties (8 bytes, 4-byte aligned)
    float shadowBias;
    float shadowIntensity;

    // Spotlight properties (8 bytes, 4-byte aligned)
    float cutoffAngle;
    float outerCutoffAngle;
};

// Input color and texture coordinate
layout(location = 0) in vec3 fragColor;
layout(location = 2) in vec2 UV;
layout(location = 1) in vec3 Normal_Face;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec4 tangent;
layout(location = 5) in vec3 cameraPos;

layout(location = 0) out vec4 outColor;


// Texture samplers sorted alphabetically by texture name
layout(binding = 1) uniform sampler2D textureSamplers[];
layout(std430, binding = 2) buffer LightBuffer {
    Light lights[];
};

// -------------------- Helpers (GGX / Fresnel / Geometry) --------------------

vec3 getNormal(vec3 normalMap)
{
    vec3 Nmap = normalize(normalMap * 2.0 - 1.0);

    vec3 N = normalize(Normal_Face);
    vec3 T = normalize(tangent.xyz - dot(tangent.xyz, N) * N);
    vec3 B = normalize(cross(N, T)) * tangent.w;

    mat3 TBN = mat3(T, B, N);

    // Transform and normalize the normal map vector into world space
    return normalize(TBN * Nmap);
}


// Normal distrobution function
float D(float alpha, vec3 N, vec3 H) {
    float numerator = pow(alpha, 2.0f);

    float NdotH = max(dot(N, H), 0.0);
    float denom = PI * pow(pow(NdotH, 2.0f) * (alpha - 1.0) + 1.0, 2.0f);

    // Prevent div by zero
    denom = max(denom, 0.00000001);

    return numerator / denom;
}


float G1(float alpha, vec3 N, vec3 X) {
    float numerator = max(dot(N, X), 0.0);


    float k = alpha / 2.0f;
    float denom = max(dot(N, X), 0.0) * (1 - k) + k;
    denom = max(denom, 0.00001);

    return numerator / denom;
}

// Geometry shadowing function
float G(float alpha, vec3 N, vec3 V, vec3 L) {
    return G1(alpha, N, V) * G1(alpha, N, L);
}

// frensnel slick function
vec3 Fresnel(vec3 F0, vec3 V, vec3 H) {
    return F0 + (vec3(1.0) - F0) * pow(1 - max(dot(V, H), 0.0), 5.0);
}


float attenuate(float distance, float intensity)
{
    // simple smooth falloff:
    float d = max(distance, 1.0);
    return 1.0f / (max(d*d, 0.00001));
}


// -------------------- Light evaluation --------------------

vec3 EvaluateLightContribution(Light light)
{
    // Data for color (Add emmisive later)
    float AO            =    texture(textureSamplers[0], UV).r;
    vec3 Albedo         =    texture(textureSamplers[1], UV).rgb;
    float Metallic      =    texture(textureSamplers[3], UV).r;
    vec3 NormalMap      =    texture(textureSamplers[4], UV).rgb;
    float Roughness     =    texture(textureSamplers[5], UV).r;

    Albedo = Albedo * fragColor;

    vec3 viewDir = normalize(cameraPos - fragPos);
    vec3 globalAmbient = vec3(0.001);
    vec3 F0 = mix(vec3(0.04), Albedo, Metallic);



    // Main information
    vec3 N = getNormal(NormalMap);
    vec3 V = normalize(cameraPos - fragPos);
    vec3 L = normalize(light.position - fragPos);

    // If using directional light use the direction from light object
    if (light.type == 3)
        L = normalize(-light.direction);

    vec3 H = normalize(V + L);

    // Rendering equation
    vec3 ks = Fresnel(F0, V, H);
    vec3 kd = vec3(1.0) - ks;

    // Apply metals
    kd = kd * (1 - Metallic);


    vec3 lambert = Albedo / PI;
    float alpha = Roughness * Roughness;

    vec3 CookTorranceNumerator = D(alpha, N, H) * G(alpha, N, V, L) * Fresnel(F0, V, H);
    float CookTorranceDenomenator = 4.0 * max(dot(V, N), 0.00) * max(dot(L, N), 0.00);
    CookTorranceDenomenator = max(CookTorranceDenomenator, 0.001);
    vec3 CookTorrance = CookTorranceNumerator / CookTorranceDenomenator;

    vec3 BRDF = kd * lambert * AO + CookTorrance;


    float intensity =  1.0f;

    // -------------------- Intensity ---------------------------
    if (light.type != 3) {
        float dist = length(light.position - fragPos);
        intensity = attenuate(dist, light.intensity);
    }


    // -------------------- Spotlight cutoff --------------------
    if (light.type == 2) { // Assume type 2 = spotlight
        float inner = cos(light.cutoffAngle * (PI / 180));
        float outer = cos(light.outerCutoffAngle * (PI / 180));

        float angle = dot(normalize(-light.direction), L);
        intensity *= clamp((angle - outer) / (inner - outer), 0.0f, 1.0f);
    }

    vec3 radiance = light.Color * intensity;

    vec3 lighting = BRDF * radiance * max(dot(L, N), 0.0);

    return lighting;
}


// -------------------- Main --------------------

void main()
{
    vec3 Emissive       =    texture(textureSamplers[2], UV).rgb;

    vec3 totalColor = vec3(0.0);
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (lights[i].type != 0)
            totalColor += EvaluateLightContribution(lights[i]);
    }

    vec3 color = totalColor + Emissive;

    // Tone mapping + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));


    // Output final color
    outColor = vec4(color, 1.0);
}