/**/
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#include "per_frame_structs.glsl"

layout(push_constant) uniform PushConstants {
    vec3 center;
    float radius;
} bounds;

const vec3 offsets[8] = vec3[8](
    vec3(1.0, 1.0, 1.0),
    vec3(-1.0, 1.0, 1.0),
    vec3(-1.0, -1.0, 1.0),
    vec3(1.0, -1.0, 1.0),
    vec3(1.0, 1.0, -1.0),
    vec3(-1.0, 1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(1.0, -1.0, -1.0)
);

const int indices[36] = int[36](
    0, 1, 2, 2, 3, 0, // Front face
    4, 5, 6, 6, 7, 4, // Back face
    0, 1, 5, 5, 4, 0, // Top face
    2, 3, 7, 7, 6, 2, // Bottom face
    0, 3, 7, 7, 4, 0, // Right face
    1, 2, 6, 6, 5, 1  // Left face
);

layout(location = 0) out vec3 vPosition;
layout(location = 1) out vec3 vCenter;

void main()
{
    int idx = indices[gl_VertexIndex];
    vec3 offset = offsets[idx] * bounds.radius;
    
    vec3 center = bounds.center;
    vec3 position = center + offset;
    
    vCenter = ( view * vec4(center, 1.0)).xyz;
    vPosition = ( view * vec4(position, 1.0)).xyz;

    gl_Position = proj * view * vec4(position, 1.0);
}

