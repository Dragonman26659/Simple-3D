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
    vec3 ambientColor;
    vec3 diffuseColor;
    vec3 specularColor;
    float intensity;

    // Shadow properties (8 bytes, 4-byte aligned)
    float shadowBias;
    float shadowIntensity;

    // Spotlight properties (8 bytes, 4-byte aligned)
    float cutoffAngle;
    float outerCutoffAngle;

    // Attenuation parameters (12 bytes, 4-byte aligned)
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
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




// Data for color
float AO            =    texture(textureSamplers[0], UV).r;
vec3 Albedo         =    texture(textureSamplers[1], UV).rgb;
float Metallic      =    texture(textureSamplers[2], UV).r;
vec3 NormalMap      =    texture(textureSamplers[3], UV).rgb;
float Roughness     =    texture(textureSamplers[4], UV).r;


vec3 viewDir = normalize(cameraPos - fragPos);
vec3 globalAmbient = vec3(0.1);


// -------------------- Helpers (GGX / Fresnel / Geometry) --------------------

vec3 getNormal()
{
    vec3 Nmap = NormalMap * 2.0 - 1.0;
    vec3 N = normalize(Normal_Face);
    vec3 T = normalize(tangent.xyz - dot(tangent.xyz, N) * N);
    vec3 B = normalize(cross(N, T)) * tangent.w;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * Nmap);
}

float D_GGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 1e-6);
}

float G_SchlickGGX(float NdotX, float roughness)
{
    // Use UE/Disney style remap for k
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = G_SchlickGGX(NdotV, roughness);
    float ggx2 = G_SchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Cook-Torrance specular (returns RGB)
vec3 CookTorranceSpecular(vec3 N, vec3 V, vec3 L, vec3 F0, float roughness)
{
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float D = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3  F = fresnelSchlick(VdotH, F0);

    // Cook-Torrance: (D * G * F) / (4 * NdotV * NdotL)
    vec3 numerator = D * G * F;
    float denom = max(4.0 * NdotV * NdotL, 0.001);
    return numerator / denom;
}


// -------------------- BRDF (energy conserving) --------------------

// Compute diffuse and specular contribution given N, V, L
void EvaluateBRDF(
    vec3 N, vec3 V, vec3 L,
    out vec3 diffuseOut, out vec3 specularOut)
{
    // Perceptual -> linear roughness is already given; we use it directly.
    float rough = clamp(Roughness, 0.0001, 1.0);

    // F0: base reflectance at normal incidence (metals use albedo)
    vec3 dielectricF0 = vec3(0.04);
    vec3 F0 = mix(dielectricF0, Albedo, Metallic);

    // Energy-conserving diffuse term (per-channel)
    vec3 kd = (vec3(1.0) - F0) * (1.0 - Metallic); // when metallic==1, kd -> 0

    // Lambert diffuse
    vec3 diffuseBRDF = kd * Albedo / PI;

    // Specular (Cook-Torrance)
    vec3 specBRDF = CookTorranceSpecular(N, V, L, F0, rough);

    diffuseOut = diffuseBRDF;
    specularOut = specBRDF;
}


// -------------------- Light evaluation --------------------

vec3 EvaluateLightContribution(Light light)
{
    vec3 N = getNormal();
    vec3 V = normalize(viewDir);
    vec3 L;
    float attenuation = 1.0;

    // Directional
    if (light.type == 3) {
        L = normalize(-light.direction);
    } else {
        vec3 toLight = light.position - fragPos;
        float dist = length(toLight);
        L = normalize(toLight);

        attenuation = 1.0 / (light.constantAttenuation +
                             light.linearAttenuation * dist +
                             light.quadraticAttenuation * dist * dist);

        // Spotlight falloff
        if (light.type == 2) {
            float theta = dot(L, normalize(-light.direction));
            float epsilon = light.cutoffAngle - light.outerCutoffAngle;
            float intensity = clamp((theta - light.outerCutoffAngle) / max(epsilon, 1e-4), 0.0, 1.0);
            attenuation *= intensity;
        }
    }

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return vec3(0.0);

    // BRDF components
    vec3 diffuseBRDF;
    vec3 specularBRDF;
    EvaluateBRDF(N, V, L, diffuseBRDF, specularBRDF);

    // Radiance from light (use diffuseColor for incoming spectral radiance)
    vec3 radiance = light.diffuseColor * light.intensity * attenuation;

    // Direct lighting: diffuse + specular
    vec3 diffuseTerm  = diffuseBRDF * radiance * NdotL;        // (kd * albedo/pi) * L * NdotL
    vec3 specularTerm = specularBRDF * light.specularColor * light.intensity * attenuation * NdotL;

    // Ambient contribution per-light (small, energy-conserving)
    // Use ambientColor to add a small ambient irradiance. For energy conservation,
    // diffuse ambient should be limited by kd and AO.
    // --- Ambient: combine per-light ambient + global ambient ---
    vec3 ambientTerm = ((light.ambientColor) + globalAmbient) * Albedo * (1.0 - Metallic);

    return ambientTerm ;
}


// -------------------- Main --------------------

void main()
{
    vec3 totalColor = vec3(0.0);
    for (int i = 0; i < lights.length(); ++i) {
        if (lights[i].type != 0)
            totalColor += EvaluateLightContribution(lights[i]);
    }

    // No extra multiplication by Albedo here
    vec3 color = totalColor;

    // Tone mapping + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}