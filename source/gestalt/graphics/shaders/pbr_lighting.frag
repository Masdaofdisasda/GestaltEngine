#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#include "per_frame_structs.glsl"
#include "input_structures.glsl"
#include "pbr.glsl"
#include "shadow_mapping.glsl"

layout(location = 0) in vec2 texCoord;

layout (location = 0) out vec4 outFragColor;

layout(set = 2, binding = 10) uniform sampler2D gbuffer1;
layout(set = 2, binding = 11) uniform sampler2D gbuffer2;
layout(set = 2, binding = 12) uniform sampler2D gbuffer3;
layout(set = 2, binding = 13) uniform sampler2D depthBuffer;
layout(set = 2, binding = 14) uniform sampler2D shadowMap;

layout(set = 3, binding = 15) buffer DirLight{
	vec3 color;
	float intensity;
	vec3 direction;
	int viewProjIndex;
} dirLight[2];

layout(set = 3, binding = 16) buffer PointLight{
	vec3 color;
	float intensity;
	vec3 position;
	bool enabled;
} pointLight[256];

layout(set = 3, binding = 17) buffer LightViewProj{
	mat4 viewProj;
} lightViewProj[256 + 2];

layout( push_constant ) uniform constants
{	
	mat4 invViewProj;
    int debugMode;
    int num_dir_lights;
    int num_point_lights;
    int shading_mode;
    int shadow_mode;
    int ibl_mode;
} params;

// Function to reconstruct world position from depth
vec3 ReconstructWorldPos(float depth, vec2 uv, mat4 invViewProj) {
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 worldSpacePosition = invViewProj * clipSpacePosition;
    return worldSpacePosition.xyz / worldSpacePosition.w;
}

// https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
vec3 SignedOctDecode(vec3 n) {

    // Perform the reverse octahedral transformation
    vec3 OutN;
    OutN.x = (n.x - n.y);
    OutN.y = (n.x + n.y) - 1.0;

    // Retrieve the sign of the z component
    OutN.z = n.z * 2.0 - 1.0;

    // Reconstruct the z component
    OutN.z = OutN.z * (1.0 - abs(OutN.x) - abs(OutN.y));

    OutN = normalize(OutN);

    return OutN;
}

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() {

    vec2 UV = texCoord;

	vec4 albedoMetal = texture(gbuffer1, UV);
	vec4 normalRough = texture(gbuffer2, UV);
	vec4 emissiveOcclusion  = texture(gbuffer3, UV);

	vec4 Kd = vec4(albedoMetal.rgb, 1.0);

	vec3 n = SignedOctDecode(normalRough.rgb);

	vec4 Ke = vec4(emissiveOcclusion.rgb, 1.0);
	
	vec4 MeR = vec4(emissiveOcclusion.a, 
					normalRough.a,
					albedoMetal.a,
					1.0);

	vec2 uv = UV;
    float depth = texture(depthBuffer, uv).r;
    vec3 worldPos = ReconstructWorldPos(depth, uv, params.invViewProj);
	vec3 viewPos = -normalize(vec3(view[0][2], view[1][2], view[2][2]));

    // Transform world position to light space
	mat4 lightViewProj = lightViewProj[dirLight[0].viewProjIndex].viewProj;
    vec4 lightSpacePos = biasMat * lightViewProj * vec4(worldPos, 1.0);

	float shadow = 1.0;
	if (params.shadow_mode == 0) {
		float bias = calculateDynamicBias(n, dirLight[0].direction);
		shadow = shadowFactor(lightSpacePos, shadowMap, bias).r;
	}

	MaterialInfo materialInfo;
	calculatePBRInputsMetallicRoughness(Kd, n, viewPos.xyz, worldPos, MeR, materialInfo);

	// Calculate lighting contribution from image based lighting source (IBL)
	if (params.ibl_mode == 0) {
		materialInfo.f_diffuse_ibl += getIBLRadianceLambertian(materialInfo);
		materialInfo.f_specular_ibl += getIBLRadianceGGX(materialInfo);
	} else if (params.ibl_mode == 1) {
		materialInfo.f_diffuse_ibl += getIBLRadianceLambertian(materialInfo);
	} else if (params.ibl_mode == 2) {
		materialInfo.f_specular_ibl += getIBLRadianceGGX(materialInfo);
	}

	// directional light contribution
	if (params.shading_mode == 0 || params.shading_mode == 1) {
		for (int i = 0; i < params.num_dir_lights; ++i) {

		vec3 pointToLight = dirLight[i].direction;

        // BSTF
		vec3 n = materialInfo.n;
		vec3 v = materialInfo.v;
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
		float NdotL = clamp(dot(n, l), 0.001, 1.0);
        float NdotV = clamp(dot(n, v), 0.0, 1.0);
		float NdotH = clamp(dot(n, h), 0.0, 1.0);
		float LdotH = clamp(dot(l, h), 0.0, 1.0);
		float VdotH = clamp(dot(v, h), 0.0, 1.0);

		if (NdotL <= 0.0 && NdotV <= 0.0) { continue; }

        // Calculation of analytical light
        // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
        vec3 intensity = getDirectionalLighIntensity(dirLight[i].color, dirLight[i].intensity);
        materialInfo.f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
        materialInfo.f_specular += shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
		}
	}

	// point light contribution
	if (params.shading_mode == 0 || params.shading_mode == 2) {
		for (int i = 0; i < params.num_point_lights; ++i) {

		vec3 pointToLight = pointLight[i].position - worldPos;

        // BSTF
		vec3 n = materialInfo.n;
		vec3 v = materialInfo.v;
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
		float NdotL = clamp(dot(n, l), 0.001, 1.0);
        float NdotV = clamp(dot(n, v), 0.0, 1.0);
		float NdotH = clamp(dot(n, h), 0.0, 1.0);
		float LdotH = clamp(dot(l, h), 0.0, 1.0);
		float VdotH = clamp(dot(v, h), 0.0, 1.0);

		if (NdotL <= 0.0 && NdotV <= 0.0) { continue; }

        // Calculation of analytical light
        // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
        vec3 intensity = getPointLighIntensity(pointLight[i].color, pointLight[i].intensity, pointToLight);
        materialInfo.f_diffuse += intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
        materialInfo.f_specular += intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
		}
	}

	materialInfo.f_emissive = Ke.rgb;

	float ao = MeR.r;
	vec3 diffuse = materialInfo.f_diffuse + materialInfo.f_diffuse_ibl * ao;
	vec3 specular = materialInfo.f_specular + materialInfo.f_specular_ibl * ao * shadow;

	vec3 color = materialInfo.f_emissive + diffuse + specular;

	if (params.debugMode == 0) {
		outFragColor = vec4(color, 1.0);
	} else if (params.debugMode == 1) {
		outFragColor = vec4(Kd.rgb, 1.0);
	} else if (params.debugMode == 2) {
		outFragColor = vec4(n * 0.5 + 0.5, 1.0);
	} else if (params.debugMode == 3) {
		outFragColor = vec4(Ke.rgb, 1.0);
	} else if (params.debugMode == 4) {
		outFragColor = vec4(MeR.rgb, 1.0);
	} else if (params.debugMode == 5) {
		outFragColor = vec4(MeR.rrr, 1.0);
	} else if (params.debugMode == 6) {
		outFragColor = vec4(0.0, MeR.g, 0.0, 1.0);
	} else if (params.debugMode == 7) {
		outFragColor = vec4(0.0, 0.0, MeR.b, 1.0);
	} else if (params.debugMode == 8) {
		outFragColor = vec4(normalize(worldPos) * 0.5 + 0.5, 1.0);
	} else if (params.debugMode == 9) {
		float normalizedDepth = depth * 2.0 - 1.0; // Assuming depth is in [0,1], map it to [-1,1]
		outFragColor = vec4(vec3(normalizedDepth), 1.0); // Visualize depth as grayscale color
	} else if (params.debugMode == 10) {
		vec3 normalizedLightSpacePos = lightSpacePos.xyz / lightSpacePos.w;
		outFragColor = vec4(normalize(normalizedLightSpacePos) * 0.5 + 0.5, 1.0);
	}





}