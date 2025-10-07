#version 450


// Change based on how many textures the material has
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



float ao            =    texture(textureSamplers[0], UV).r;
vec3 albedo         =    texture(textureSamplers[1], UV).rgb;
float metallic      =    texture(textureSamplers[2], UV).r;
vec3 Normal         =    texture(textureSamplers[3], UV).rgb;
float roughness     =    texture(textureSamplers[4], UV).r;



// Normal at frag coord
vec3 getNormal()
{    
    Normal = Normal * 2.0 - 1.0; // convert from [0,1] → [-1,1]

    // Retrieve interpolated vertex data
    vec3 N = normalize(Normal_Face);
    vec3 T = normalize(tangent.xyz - dot(tangent.xyz, N) * N); // orthogonalize
    vec3 B = normalize(cross(N, T)) * tangent.w;               // apply handedness

    // Transform tangent-space normal to world-space
    mat3 TBN = mat3(T, B, N);
    vec3 worldNormal = normalize(TBN * Normal);

    return worldNormal;
}


void main() {
    vec4 texColor = texture(textureSamplers[1], UV);
    vec4 fragColor = texColor;

    // Sample the texture at the current coordinates
    for (int i = 0; i < lights.length(); i++) {
        Light light = lights[i];

        // Uninitalised lights have a type of 0
        if (light.type == 0)
            continue; 

        vec3 normal = getNormal();
        vec3 lightDir = normalize(light.position - fragPos);

        float diffuse = max(dot(normal, lightDir), 0.0f);

        fragColor = ((texColor * 0.02f) + (vec4(light.diffuseColor, 1.0f) * diffuse) / length(light.position - fragPos));
    }


    
    outColor = fragColor;
}