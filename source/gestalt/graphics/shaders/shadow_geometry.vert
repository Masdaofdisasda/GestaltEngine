#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require

#include "per_frame_structs.glsl"

layout(set = 1, binding = 2) buffer LightViewProj{
	mat4 viewProj;
} lightViewProj[256 + 2];


struct VertexPosition {

	vec3 position;
	float pad;
}; 

layout(set=2, binding = 0) readonly buffer VertexPositionBuffer{ 
	VertexPosition positions[];
} vertexPositionBuffer;

//push constants block
layout( push_constant ) uniform constants
{
	mat4 model_matrix;
	int materialIndex;
} PushConstants;

void main() 
{
	VertexPosition v = vertexPositionBuffer.positions[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);
	
	mat4 lightViewProj = lightViewProj[0].viewProj;
    gl_Position = lightViewProj * PushConstants.model_matrix * position;
}