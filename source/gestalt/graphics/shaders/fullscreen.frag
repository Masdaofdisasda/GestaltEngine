#version 450

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;


layout(set = 0, binding = 10) uniform sampler2D inputTexture;


void main() {
    vec4 texColor = texture(inputTexture, TexCoords);

    // Apply post-processing effects here
    // For now, just output the sampled color
    FragColor = texColor.rrra;

    // Example for simple tonemapping (extend as needed)
    // outColor.rgb = outColor.rgb / (outColor.rgb + vec3(1.0));
}

