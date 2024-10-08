/**/
#version 460

layout(location = 0) in vec2 texCoord;
layout(location = 0) out float outColor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

void main() {

    vec3 linearRGB = texture(texSampler, texCoord).rgb;

    // Convert the linear RGB color to luminance using the Rec. 709 formula.
    float luminance = dot(linearRGB, vec3(0.2126, 0.7152, 0.0722));

    outColor = luminance;
}
