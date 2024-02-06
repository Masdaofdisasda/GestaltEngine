/**/
#version 460

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
	bool showBrightPass;
    float exposure;
	float maxWhite; 
	float bloomStrength; 
	float adaptationSpeed;
} params;

layout(binding = 10) uniform sampler2D texScene;
layout(binding = 11) uniform sampler2D texBloom;
//layout(binding = 12) uniform sampler2D texLuminance; TODO

// Extended Reinhard tone mapping operator
vec3 Reinhard2(vec3 x)
{
	return (x * (1.0 + x / (params.maxWhite * params.maxWhite))) / (1.0 + x);
}

void main()
{
	vec3 color = texture(texScene, uv).rgb;
	vec3 bloom = texture(texBloom, vec2(uv.x, uv.y)).rgb;

	if (params.showBrightPass)
	{
		outColor = vec4(bloom, 1.0);
		return;
	}

	//float avgLuminance = texture(texLuminance, vec2(0.5, 0.5)).x;

	float midGray = 0.5;

	//color *= ubo.exposure * midGray / (avgLuminance + 0.001);
	color = Reinhard2(color);
	outColor = vec4(color + params.bloomStrength * bloom, 1.0);
}