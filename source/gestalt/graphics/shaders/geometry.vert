#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "per_frame_structs.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outPosition; //was color
layout (location = 2) out vec2 outUV;
layout (location = 3) flat out int outMaterialIndex;
layout (location = 4) flat out int outMaterialConst;

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
	mat4 render_matrix;
	int materialIndex;
	int materialConst;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position =  sceneData.viewproj * PushConstants.render_matrix * position;

	outNormal = (PushConstants.render_matrix * vec4(v.normal, 0.f)).xyz;
	//outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outPosition = vec3(PushConstants.render_matrix * vec4(v.position, 1.0));
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outMaterialIndex = PushConstants.materialIndex;
	outMaterialConst = PushConstants.materialConst;
}