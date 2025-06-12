#version 450
// Change based on how many textures the material has
#define MAX_TEXTURES 5
#define MAX_LIGHTS 256
#define PI         3.14159265359

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
layout(location = 2) in vec2 fragTexCoord;
layout(location = 1) in vec3 Normal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;


// Texture samplers sorted alphabetically by texture name
layout(binding = 1) uniform sampler2D textureSamplers[MAX_TEXTURES];
layout(std140, binding = 2) uniform LightBuffer { Light lights[MAX_LIGHTS]; };
layout(binding = 3) uniform sampler2D shadowMaps[MAX_LIGHTS];


//––– Camera ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
uniform vec3 cameraPos;

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
    vec3 Nmap = texture(textureSamplers[3], UV).xyz*2.0 -1.0;
    mat3 TBN = mat3(normalize(Tangent),
                    normalize(Bitangent),
                    normalize(Normal));
    return normalize(TBN * Nmap);
}

//––– Shadow-map lookup (3×3 PCF) ––––––––––––––––––––––––––––––––––––––––––––––
float calcShadow(int idx, vec3 N, vec3 Ldir) {
    Light L = lights[idx];
    if (!L.castShadows) return 0.0;

    vec4 proj4 = lightSpace[idx] * vec4(fragPos, 1.0);
    vec3 proj  = proj4.xyz / proj4.w;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) 
        return 0.0;

    float bias     = max(L.shadowBias * (1.0 - dot(N, Ldir)), 0.005);
    float texelSize= 1.0 / textureSize(shadowMaps[idx], 0).x;
    float sum      = 0.0;
    for(int x=-1; x<=1; ++x)
    for(int y=-1; y<=1; ++y) {
        vec2 ofs = vec2(x,y)*texelSize;
        sum += texture(shadowMaps[idx], vec3(proj.xy + ofs, proj.z - bias));
    }
    sum /= 9.0;
    return clamp(sum * L.shadowIntensity, 0.0, 1.0);
}

//––– Main –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
void main() {
    // fetch material
    vec3  albedo    = pow(texture(textureSamplers[0], UV).rgb, vec3(2.2));
    float metallic  = texture(textureSamplers[2], UV).r;
    float roughness = texture(textureSamplers[4], UV).r;
    float ao        = texture(textureSamplers[1], UV).r;

    // geometry
    vec3 N  = getNormal();
    vec3 V  = normalize(cameraPos - fragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // lighting loop
    vec3 Lo = vec3(0.0);
    for(int i=0; i<MAX_LIGHTS; ++i) {
        Light L = lights[i];
        if(L.type==0) continue;

        vec3 Ldir    = normalize(L.position - fragPos);
        vec3 H       = normalize(V + Ldir);
        float dist   = length(L.position - fragPos);
        float atten  = 1.0/(L.constantAttenuation
                         +L.linearAttenuation*dist
                         +L.quadraticAttenuation*dist*dist);
        vec3 radiance = L.diffuseColor * L.intensity * atten;

        // Cook–Torrance
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, Ldir, roughness);
        vec3  F   = fresnelSchlick(max(dot(H,V),0.0), F0);
        vec3  nom = NDF * G * F;
        float denom = 4.0*max(dot(N,V),0.0)*max(dot(N,Ldir),0.0) + 1e-4;
        vec3 spec   = nom/denom;

        vec3 kS = F;
        vec3 kD = (1.0 -kS)*(1.0 -metallic);
        float NdotL = max(dot(N,Ldir), 0.0);

        // shadow factor [0..1]
        float sh = calcShadow(i, N, Ldir);

        // accumulate
        Lo += (kD * albedo/PI + spec) * radiance * NdotL * (1.0 - sh);
    }

    // ambient + AO + tone‐map + gamma
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo;
    color = color/(color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
