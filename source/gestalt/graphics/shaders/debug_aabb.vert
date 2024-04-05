/**/
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#include "per_frame_structs.glsl"

layout(push_constant) uniform PushConstants {
    vec4 aabbMin;
    vec4 aabbMax;
} bounds;

const int indices[36] = int[36](
	0, 1, 2, 2, 3, 0,
	1, 5, 6, 6, 2, 1,
	7, 6, 5, 5, 4, 7,
	4, 0, 3, 3, 7, 4,
	4, 5, 1, 1, 0, 4,
	3, 2, 6, 6, 7, 3
);

void main()
{
	vec3 aabbMin = vec3(bounds.aabbMin);
	vec3 aabbMax = vec3(bounds.aabbMax);

	const vec3 pos[8] = vec3[8](
	vec3(aabbMin.x, aabbMin.y, aabbMax.z),
	vec3(aabbMax.x, aabbMin.y, aabbMax.z),
	vec3(aabbMax.x, aabbMax.y, aabbMax.z),
	vec3(aabbMin.x, aabbMax.y, aabbMax.z),

	vec3(aabbMin.x, aabbMin.y, aabbMin.z),
	vec3(aabbMax.x, aabbMin.y, aabbMin.z),
	vec3(aabbMax.x, aabbMax.y, aabbMin.z),
	vec3(aabbMin.x, aabbMax.y, aabbMin.z)
	);

	int idx = indices[gl_VertexIndex];

	gl_Position = sceneData.viewproj * vec4(pos[idx], 1.0);
}

