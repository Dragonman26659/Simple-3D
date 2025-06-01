#version 450


// Change based on how many textures the material has
#define MAX_TEXTURES 1
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
layout(location = 2) in vec2 fragTexCoord;
layout(location = 1) in vec3 Normal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;


// Texture samplers sorted alphabetically by texture name
layout(binding = 1) uniform sampler2D textureSamplers[MAX_TEXTURES];
layout(std140, binding = 2) uniform LightBuffer { Light lights[MAX_LIGHTS]; };



void main() {
    vec4 texColor = texture(textureSamplers[0], fragTexCoord);
    vec4 fragColor = texColor;

    // Sample the texture at the current coordinates
    for (int i = 0; i < lights.length(); i++) {
        Light light = lights[i];

        // Uninitalised lights have a type of 0
        if (light.type == 0)
            continue; 

        vec3 normal = normalize(Normal);
        vec3 lightDir = normalize(light.position - fragPos);

        float diffuse = max(dot(normal, lightDir), 0.0f);

        fragColor = (texColor * vec4(light.diffuseColor, 1.0f) * diffuse) + (vec4(light.ambientColor, 1.0f) * 0.02f);
    }


    
    outColor = fragColor;
}