#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#include "per_frame_structs.glsl"
#include "input_structures.glsl"
#include "normal_mapping.glsl"

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inNormal_BiTanX;
layout (location = 2) in vec4 inTangent_BiTanY;
layout (location = 3) in vec4 inPosition_BiTanZ;
layout (location = 4) flat in int inMaterialIndex;

layout(location = 0) out vec4 gBuffer1; // Albedo (RGB) + Metalness (A)
layout(location = 1) out vec4 gBuffer2; // Normal (RG) + Roughness (A)
layout(location = 2) out vec4 gBuffer3; // Emissive (RGB) + Occlusion (A)


vec3 sRGBToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + vec3(0.055)) / vec3(1.055), vec3(2.4)), step(vec3(0.04045), color));
}

void main() {

	uint16_t albedoIndex =			materialData[nonuniformEXT(inMaterialIndex)].albedo_tex_index;
	uint16_t metalicRoughIndex =	materialData[nonuniformEXT(inMaterialIndex)].metal_rough_tex_index;
	uint16_t normalIndex =			materialData[nonuniformEXT(inMaterialIndex)].normal_tex_index;
	uint16_t emissiveIndex =		materialData[nonuniformEXT(inMaterialIndex)].emissive_tex_index;
	uint16_t occlusionIndex =		materialData[nonuniformEXT(inMaterialIndex)].occlusion_tex_index;

    vec2 UV = inUV;
	vec3 inNormal = normalize(inNormal_BiTanX.xyz);
	vec3 inTangent = normalize(inTangent_BiTanY.xyz);
	vec3 inBiTangent = normalize(inPosition_BiTanZ.xyz);
	vec3 inPosition = inPosition_BiTanZ.xyz;

	vec4 Kd = materialData[nonuniformEXT(inMaterialIndex)].albedo_factor;
	if(albedoIndex != uint(-1)) {
		Kd = texture(nonuniformEXT(textures[albedoIndex]), UV);
    }
	Kd.rgb = sRGBToLinear(Kd.rgb);

	float alphaCutoff = materialData[nonuniformEXT(inMaterialIndex)].alpha_cutoff;
    if (Kd.a < alphaCutoff) {
        discard;
    }

	vec3 n = normalize(inNormal);
	vec3 viewPos = -normalize(vec3(sceneData.view[0][2], sceneData.view[1][2], sceneData.view[2][2]));
	if(normalIndex != uint(-1)) {
		vec3 normal_sample = texture(nonuniformEXT(textures[normalIndex]), UV).rgb;
		n = perturbNormal(n, normalize(viewPos - inPosition), normal_sample, UV);
	}

	vec4 Ke = vec4(materialData[nonuniformEXT(inMaterialIndex)].emissiveColor, 1.0);
	if(emissiveIndex != uint(-1)) {
		Ke = texture(nonuniformEXT(textures[emissiveIndex]), UV);
	}
		Ke.rgb *= materialData[nonuniformEXT(inMaterialIndex)].emissiveColor;
		Ke.rgb = sRGBToLinear(Ke.rgb);

	float Kao = 1.0;
	if (occlusionIndex != uint(-1)) {
		Kao = texture(nonuniformEXT(textures[occlusionIndex]), UV).r;
	}
	
	vec4 MeR = vec4(Kao, materialData[nonuniformEXT(inMaterialIndex)].metal_rough_factor, 1.0);
	if (metalicRoughIndex != uint(-1)) {
		MeR = texture(nonuniformEXT(textures[metalicRoughIndex]), UV);
	}
	
	gBuffer1 = vec4(Kd.rgb, MeR.b); // Albedo + Metalness
    gBuffer2 = vec4(normalize(n) * 0.5 + 0.5, MeR.g); // Normal + Roughness
    gBuffer3 = vec4(Ke.rgb, Kao); // Emissive + Occlusion

}