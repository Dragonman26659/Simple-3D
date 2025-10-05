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
layout(location = 1) in vec3 Normal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 cameraPos;

layout(location = 0) out vec4 outColor;


// Texture samplers sorted alphabetically by texture name
layout(binding = 1) uniform sampler2D textureSamplers[];
layout(std430, binding = 2) buffer LightBuffer {
    Light lights[];
};
//layout(binding = 3) uniform sampler2D shadowMaps[MAX_LIGHTS]; - Use instanced textures for this



//––– PBR helpers –––––––––––––––––––––––––––––––––––––––––––––––––––––––––
float DistributionGGX(vec3 N, vec3 H, float rou) {
    float a   = rou*rou;
    float a2  = a*a;
    float NdotH = max(dot(N,H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float denom = (NdotH2*(a2 -1.0) +1.0);
            denom = PI * denom * denom;
    return a2/denom;
}

float GeometrySchlickGGX(float NdotV, float rou) {
    float r = rou+1.0;
    float k = (r*r)/8.0;
    return NdotV/(NdotV*(1.0-k)+k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float rou) {
    float ggx2 = GeometrySchlickGGX(max(dot(N,V),0.0), rou);
    float ggx1 = GeometrySchlickGGX(max(dot(N,L),0.0), rou);
    return ggx1*ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 -F0)*pow(1.0 - cosTheta, 5.0);
}

vec3 getNormal() {
    vec3 Nmap = texture(textureSamplers[1], UV).xyz * 2.0 - 1.0;
    vec3 T = normalize(tangent);
    vec3 N = normalize(Normal);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return Normal;
}

//––– Main –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
void main() {
    vec3 albedo    = texture(textureSamplers[0], UV).rgb;
    float roughness = texture(textureSamplers[2], UV).r;
    float metallic = 0.0;
    float ao       = 1.0;

    vec3 N  = getNormal();
    vec3 V  = normalize(cameraPos - fragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i=0; i<MAX_LIGHTS; ++i) {
        Light L = lights[i];
        if(L.type == 0) continue;

        vec3 Ldir;
        if(L.type == 1) { // point light
            Ldir = normalize(L.position - fragPos);
        } else if(L.type == 2) { // directional light
            Ldir = normalize(-L.direction);
        } else {
            continue;
        }

        float dist = length(L.position - fragPos);
        float atten = 1.0 / max(L.constantAttenuation + L.linearAttenuation*dist + L.quadraticAttenuation*dist*dist, 1e-4);
        vec3 radiance = L.diffuseColor * L.intensity * atten;

        vec3 H = normalize(V + Ldir);
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, Ldir, roughness);
        vec3 F    = fresnelSchlick(max(dot(H,V),0.0), F0);
        vec3 nom  = NDF * G * F;
        float denom = 4.0*max(dot(N,V),0.0)*max(dot(N,Ldir),0.0) + 1e-4;
        vec3 spec = nom / denom;

        vec3 kS = F;
        vec3 kD = (1.0 - kS)*(1.0 - metallic);
        float NdotL = max(dot(N,Ldir),0.0);

        Lo += (kD*albedo/PI + spec) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0)); // tone mapping
    color = pow(color, vec3(1.0/2.2));   // gamma

    outColor = vec4(color, 1.0);
}