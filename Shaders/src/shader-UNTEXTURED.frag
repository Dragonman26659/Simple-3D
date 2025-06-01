#version 450


// Input color and texture coordinate
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;



void main() {
    outColor = vec4(fragColor, 1.0);
}