#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require

#include "per_frame_structs.glsl"

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outNormal_BiTanX;
layout (location = 2) out vec4 outTangent_BiTanY;
layout (location = 3) out vec4 outPosition_BiTanZ;
layout (location = 4) flat out int outMaterialIndex;

struct VertexPosition {
	vec3 position;
	float pad;
}; 

struct VertexData {
    uint8_t nx, ny, nz, nw; // normal
    uint8_t tx, ty, tz, tw; // tangent
    float16_t tu, tv;       // tex coords
	float pad;
};

layout(set=4, binding = 0) readonly buffer VertexPositionBuffer{ 
	VertexPosition positions[];
} vertexPositionBuffer;

layout(set=4, binding = 1) readonly buffer VertexDataBuffer{ 
	VertexData vertex_data[];
} vertexDataBuffer;

layout( push_constant ) uniform constants
{
	mat4 model_matrix;
	int materialIndex;
} PushConstants;

const float i8_inverse = 1.0 / 127.0;

void main() 
{
	VertexPosition v = vertexPositionBuffer.positions[gl_VertexIndex];
	VertexData data = vertexDataBuffer.vertex_data[gl_VertexIndex];
	
	outUV = vec2(data.tu, data.tv);

	vec4 position = vec4(v.position, 1.0f);
	outPosition_BiTanZ.xyz = vec3(PushConstants.model_matrix * position);
	gl_Position =  sceneData.viewproj * PushConstants.model_matrix * position;

	vec3 normal = vec3(int(data.nx), int(data.ny), int(data.nz)) * i8_inverse - 1.0;
	outNormal_BiTanX.xyz = normalize(mat3(PushConstants.model_matrix) * normal);

	vec3 tangent = vec3(int(data.tx), int(data.ty), int(data.tz)) * i8_inverse - 1.0;
	outTangent_BiTanY.xyz = normalize(mat3(PushConstants.model_matrix) * tangent);

	vec3 bitangent = cross( outNormal_BiTanX.xyz, tangent ) * (int(data.tw) * i8_inverse  - 1.0 );
    outNormal_BiTanX.w = bitangent.x;
    outTangent_BiTanY.w = bitangent.y;
    outPosition_BiTanZ.w = bitangent.z;

	outMaterialIndex = PushConstants.materialIndex;
}
