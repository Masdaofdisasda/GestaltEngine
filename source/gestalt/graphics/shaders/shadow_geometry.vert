#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "per_frame_structs.glsl"

struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 model_matrix;
	int materialIndex;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);
	
    gl_Position = sceneData.lightViewProj * PushConstants.model_matrix * position;
}