/**/
#version 460

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform BlurDirection {
    int direction; // 0 for X, 1 for Y
} blurDir;

const vec4 gaussFilter[11] = vec4[11](
	vec4(-5.0, 0.0, 0.0,  3.0/133.0),
	vec4(-4.0, 0.0, 0.0,  6.0/133.0),
	vec4(-3.0, 0.0, 0.0, 10.0/133.0),
	vec4(-2.0, 0.0, 0.0, 15.0/133.0),
	vec4(-1.0, 0.0, 0.0, 20.0/133.0),
	vec4( 0.0, 0.0, 0.0, 25.0/133.0),
	vec4( 1.0, 0.0, 0.0, 20.0/133.0),
	vec4( 2.0, 0.0, 0.0, 15.0/133.0),
	vec4( 3.0, 0.0, 0.0, 10.0/133.0),
	vec4( 4.0, 0.0, 0.0,  6.0/133.0),
	vec4( 5.0, 0.0, 0.0,  3.0/133.0)
);

void main()
{
    const float texScaler =  2.0 / 512.0;
    const float texOffset = -0.5 / 512.0;

    vec4 Color = vec4(0.0);
    for (int i = 0; i < 11; i++) {
        vec2 Coord;
        if (blurDir.direction == 0) {
            Coord = vec2(texCoord.x + gaussFilter[i].x * texScaler + texOffset, texCoord.y);
        } else {
            Coord = vec2(texCoord.x, texCoord.y + gaussFilter[i].x * texScaler + texOffset);
        }

        Color += texture(texSampler, Coord) * gaussFilter[i].w;
    }

    outColor = vec4(Color.xyz, 1.0);
}
