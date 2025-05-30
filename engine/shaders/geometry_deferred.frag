#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#extension GL_GOOGLE_include_directive : require
#include "per_frame_structs.glsl"

layout(set = 1, binding = 3) uniform sampler2D textures[1280];

struct MaterialConstants {
    uint16_t albedo_tex_index;
    uint16_t metal_rough_tex_index;
    uint16_t normal_tex_index;
    uint16_t emissive_tex_index;
    uint16_t occlusion_tex_index;
    uint textureFlags;

    vec4 albedo_factor;
    vec2 metal_rough_factor;
    float occlusionStrength;
    float alpha_cutoff;
    vec3 emissiveColor;
    float emissiveStrength; 
};

layout(std430, set = 1, binding = 4) readonly buffer MaterialData {
    MaterialConstants materialData[256];
};
#include "normal_mapping.glsl"

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inNormal_BiTanX;
layout (location = 2) in vec4 inTangent_BiTanY;
layout (location = 3) in vec4 inPosition_BiTanZ;
layout (location = 4) flat in int inMaterialIndex;

layout(location = 0) out vec4 gBuffer1; // Albedo (RGB) + Metalness (A)
layout(location = 1) out vec4 gBuffer2; // Normal (RG) + Roughness (A)
layout(location = 2) out vec4 gBuffer3; // Emissive (RGB) + Occlusion (A)

#define FLT_MAX 3.402823466e+38

vec3 sRGBToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + vec3(0.055)) / vec3(1.055), vec3(2.4)), step(vec3(0.04045), color));
}

// https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
vec3 SignedOctEncode(vec3 n) {
	n = normalize(n);

    // Perform the octahedral transformation
    vec3 OutN;
    OutN.y = n.y * 0.5 + 0.5;
    OutN.x = n.x * 0.5 + OutN.y;
    OutN.y = n.x * -0.5 + OutN.y;

    // Store the sign of the z component
    OutN.z = clamp(n.z * FLT_MAX, 0.0, 1.0);

    return OutN;
}

void main() {

	uint16_t albedoIndex =			materialData[nonuniformEXT(inMaterialIndex)].albedo_tex_index;
	uint16_t metalicRoughIndex =	materialData[nonuniformEXT(inMaterialIndex)].metal_rough_tex_index;
	uint16_t normalIndex =			materialData[nonuniformEXT(inMaterialIndex)].normal_tex_index;
	uint16_t emissiveIndex =		materialData[nonuniformEXT(inMaterialIndex)].emissive_tex_index;
	uint16_t occlusionIndex =		materialData[nonuniformEXT(inMaterialIndex)].occlusion_tex_index;
	

	uint textureFlags = materialData[nonuniformEXT(inMaterialIndex)].textureFlags;

	bool hasAlbedoTexture = (textureFlags & 0x01) != 0;
	bool hasMetalRoughTexture = (textureFlags & 0x02) != 0;
	bool hasNormalTexture = (textureFlags & 0x04) != 0;
	bool hasEmissiveTexture = (textureFlags & 0x08) != 0;
	bool hasOcclusionTexture = (textureFlags & 0x10) != 0;

    vec2 UV = inUV;
	vec3 inNormal = normalize(inNormal_BiTanX.xyz);
	vec3 inTangent = normalize(inTangent_BiTanY.xyz);
	vec3 inBiTangent = normalize(vec3(inNormal_BiTanX.w, inTangent_BiTanY.w, inPosition_BiTanZ.w));
	vec3 inPosition = inPosition_BiTanZ.xyz;

	vec4 Kd = materialData[nonuniformEXT(inMaterialIndex)].albedo_factor;
	if(hasAlbedoTexture) {
		Kd = texture(textures[nonuniformEXT(albedoIndex)], UV);
    }
	Kd.rgb = sRGBToLinear(Kd.rgb);

	float alphaCutoff = materialData[nonuniformEXT(inMaterialIndex)].alpha_cutoff;
    if (Kd.a < alphaCutoff) {
        discard;
    }

	vec3 n = normalize(inNormal);
	vec3 viewPos = -normalize(vec3(view[0][2], view[1][2], view[2][2]));
	if(hasNormalTexture) {
		vec3 normal_sample = texture(nonuniformEXT(textures[normalIndex]), UV).rgb;
		n = perturbNormal(n, inPosition, normal_sample, UV);
	}

	vec4 Ke = vec4(materialData[nonuniformEXT(inMaterialIndex)].emissiveColor, 1.0);
	if(hasEmissiveTexture) {
		Ke = texture(nonuniformEXT(textures[emissiveIndex]), UV);
	}
	Ke.rgb *= materialData[nonuniformEXT(inMaterialIndex)].emissiveStrength;
	Ke.rgb = sRGBToLinear(Ke.rgb);

	float Kao = 1.0;
	if (hasOcclusionTexture) {
		Kao = texture(nonuniformEXT(textures[occlusionIndex]), UV).r;
	}
	
	vec4 MeR = vec4(Kao, materialData[nonuniformEXT(inMaterialIndex)].metal_rough_factor, 1.0);
	if (hasMetalRoughTexture) {
		MeR = texture(nonuniformEXT(textures[metalicRoughIndex]), UV);
	}
	
	gBuffer1 = vec4(Kd.rgb, MeR.b); // Albedo + Metalness
    //gBuffer2 = vec4(SignedOctEncode(n), MeR.g); // Normal + Roughness
    gBuffer2 = vec4(n, MeR.g); // Normal + Roughness
    gBuffer3 = vec4(Ke.rgb, Kao); // Emissive + Occlusion

}