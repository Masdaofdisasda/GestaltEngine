#version 450

#extension GL_GOOGLE_include_directive : require
#include "per_frame_structs.glsl"
#include "input_structures.glsl"

layout(location = 0) in vec3 TexCoords;
layout(location = 0) out vec4 FragColor;

void main() {
    vec3 skyColor = texture(texEnvMap, TexCoords).rgb;

    FragColor = vec4(skyColor, 1.0);
}
