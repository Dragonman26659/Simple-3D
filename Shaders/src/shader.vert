#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in int textureID;

layout(push_constant) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(location = 0) out vec3 fragColor;      // Declare output color
layout(location = 1) out vec3 normalOut;      // Normal output
layout(location = 2) out vec2 fragTexCoord;   // Declare output texcoord
layout(location = 3) out vec3 fragPos;      // Position output
layout(location = 4) out vec4 tangent;      // Position output
layout(location = 5) out vec3 cameraPos;      // Position output

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

    
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    normalOut = normal;
    tangent = inTangent;
    cameraPos = vec3(ubo.cameraPos);
    fragPos = vec3(ubo.model * vec4(inPosition, 1.0f));
}