#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require

#include "per_frame_structs.glsl"

layout(location = 0) out vec3 TexCoords;

const vec3 cubeVertices[36] = vec3[](
    // Front face
    vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3(1.0, -1.0, -1.0),
    vec3(1.0, -1.0, -1.0), vec3(1.0,  1.0, -1.0), vec3(-1.0,  1.0, -1.0),

    // Back face
    vec3(1.0,  1.0, 1.0), vec3(1.0, -1.0, 1.0), vec3(-1.0, -1.0, 1.0),
    vec3(-1.0, -1.0, 1.0), vec3(-1.0,  1.0, 1.0), vec3(1.0,  1.0, 1.0),

    // Right face
    vec3(1.0,  1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, -1.0, 1.0),
    vec3(1.0, -1.0, 1.0), vec3(1.0,  1.0, 1.0), vec3(1.0,  1.0, -1.0),

    // Left face
    vec3(-1.0,  1.0, 1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, -1.0, -1.0),
    vec3(-1.0, -1.0, -1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0,  1.0, 1.0),

    // Top face
    vec3(-1.0, 1.0,  1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, 1.0, -1.0),
    vec3(1.0, 1.0, -1.0), vec3(1.0, 1.0,  1.0), vec3(-1.0, 1.0,  1.0),

    // Bottom face
    vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0,  1.0), vec3(1.0, -1.0,  1.0),
    vec3(1.0, -1.0,  1.0), vec3(1.0, -1.0, -1.0), vec3(-1.0, -1.0, -1.0)
);

void main() {
    TexCoords = cubeVertices[gl_VertexIndex].xyz;

    // Remove the translation from the view matrix
    mat4 viewRotationOnly = view;
    viewRotationOnly[3][0] = 0.0;
    viewRotationOnly[3][1] = 0.0;
    viewRotationOnly[3][2] = 0.0;

    // Transform the vertex positions
    vec4 pos = proj * viewRotationOnly * vec4(cubeVertices[gl_VertexIndex], 1.0);

    // Set the depth value to the far plane to ensure the skybox is always rendered behind other objects
    gl_Position = pos.xyww;
}
