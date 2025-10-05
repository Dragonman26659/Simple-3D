#version 450


// Input color and texture coordinate
// Input color and texture coordinate
layout(location = 0) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 1) in vec3 Normal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 cameraPos;

layout(location = 0) out vec4 outColor;


// Texture samplers sorted alphabetically by texture name
layout(binding = 1) uniform sampler2D textureSamplers[];



void main() {
    // Sample the texture at the current coordinates
    vec4 texColor = texture(textureSamplers[0], fragTexCoord);
    vec4 finalColor = texColor.a < 0.0 ? vec4(fragColor, 1.0) : texColor;
    outColor = finalColor;
}