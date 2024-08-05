#version 450

layout(location = 0) in  vec2 texCoord1;
layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
    bool show_ssao_only;
	float scale;
	float bias;
	float zNear;
	float zFar;
	float radius;
	float attScale;
	float distScale;
} params;

layout(binding = 0) uniform sampler2D texScene;
layout(binding = 1) uniform sampler2D texSSAO;

void main()
{
	vec2 uv = vec2(texCoord1.x, 1.0 - texCoord1.y);
	float ssao = clamp( texture(texSSAO,  uv).r + params.bias, 0.0, 1.0 );
	ssao = pow(ssao, params.scale); 

	if (params.show_ssao_only)
	{
		outColor = vec4(vec3(ssao), 1.0);
		return;
	}

	vec4 color = texture(texScene, texCoord1);

	outColor = vec4(
		(color * ssao).rgb,
		1.0
	);
}
