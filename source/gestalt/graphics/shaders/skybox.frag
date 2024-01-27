#version 450
layout(location = 0) in vec3 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform samplerCube SkyboxTexture;

void main() {
    FragColor = texture(SkyboxTexture, TexCoords);
}
