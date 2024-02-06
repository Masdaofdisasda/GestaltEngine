#version 450

layout(location = 0) out vec2 TexCoords;

const vec2 triangleVertices[3] = vec2[](vec2(-1.0, -3.0),
                                      vec2(-1.0,  1.0),
                                      vec2( 3.0,  1.0));

void main() {
    vec2 position = triangleVertices[gl_VertexIndex];

    // Map from [-1, 1] to [0, 1] range for texture coordinates
    TexCoords = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
