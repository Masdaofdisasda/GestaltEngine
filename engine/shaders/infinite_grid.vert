//
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#include "per_frame_structs.glsl"

// extents of grid in world coordinates
float gridSize = 1000.0;

const vec3 pos[4] = vec3[4](
	vec3(-1.0, 0.0, -1.0),
	vec3( 1.0, 0.0, -1.0),
	vec3( 1.0, 0.0,  1.0),
	vec3(-1.0, 0.0,  1.0)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

layout (location=0) out vec4 uv;

void main()
{
	mat4 MVP = proj * view; // * ubo.model;

	int idx = indices[gl_VertexIndex];
	vec3 position = pos[idx] * gridSize;

	mat4 iview = inverse(view);
	vec2 cameraPos = vec2(iview[3][0], iview[3][2]);

	position.x += cameraPos.x;
	position.z += cameraPos.y;

	gl_Position = MVP * vec4(position, 1.0);
	uv.yx = position.xz;
	uv.wz = position.xz;
}