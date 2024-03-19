//
#version 460

#extension GL_GOOGLE_include_directive : require
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
layout (location=1) out vec2 cameraPos;

void main()
{
	mat4 MVP = sceneData.viewproj; // * ubo.model;

	int idx = indices[gl_VertexIndex];
	vec3 position = pos[idx] * gridSize;

	mat4 iview = inverse(sceneData.view);
	cameraPos = vec2(iview[3][0], iview[3][2]);

	position.x += cameraPos.x;
	position.z += cameraPos.y;

	gl_Position = MVP * vec4(position, 1.0);
	uv.yx = position.xz;
	uv.wz = position.xz;
}


/*
void main()
{
	mat4 MVP = sceneData.viewproj;

	int idx = indices[gl_VertexIndex];
	vec3 position = pos[idx] * gridSize;
	gl_Position = MVP * vec4(position, 1.0);
	
	float div = max(2.0, round(10.0));

	vec3 worldPos = (sceneData.view * vec4(position, 1.0)).xyz;
	vec3 viewPos = -normalize(vec3(sceneData.view[0][2], sceneData.view[1][2], sceneData.view[2][2]));
	vec3 cameraCenteringOffset = floor(viewPos / div) * div;

	mat4 iview = inverse(sceneData.view);
	cameraPos = vec2(iview[3][0], iview[3][2]);

	uv.yx = (worldPos - cameraCenteringOffset).xz;
	uv.wz = worldPos.xz;
}*/