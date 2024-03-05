#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
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
layout(set = 2, binding = 15) buffer DirLight{
	vec3 color;
	float intensity;
	vec3 direction;
	bool enabled;
} dirLight[2];

layout(set = 2, binding = 16) buffer PointLight{
	vec3 color;
	float intensity;
	vec3 position;
	bool enabled;
} pointLight[256];

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

	vec3 n = normalize(normalRough.rgb * 2.0 - 1.0);

	vec4 Ke = vec4(emissiveOcclusion.rgb, 1.0);
	
	vec4 MeR = vec4(emissiveOcclusion.a, 
					normalRough.a,
					albedoMetal.a,
					1.0);

	vec2 uv = UV;
    float depth = texture(depthBuffer, uv).r;
    vec3 worldPos = ReconstructWorldPos(depth, uv, params.invViewProj);
	vec3 viewPos = -normalize(vec3(sceneData.view[0][2], sceneData.view[1][2], sceneData.view[2][2]));

    // Transform world position to light space
    vec4 lightSpacePos = biasMat * sceneData.lightViewProj * vec4(worldPos, 1.0);

	float shadow = 1.0;
	if (params.shadow_mode == 0) {
		float bias = calculateDynamicBias(n, dirLight[0].direction);
		shadow = shadowFactor(lightSpacePos, shadowMap, bias).r;
	}

	PBRInfo pbrInputs;
	calculatePBRInputsMetallicRoughness(Kd, n, viewPos.xyz, worldPos, MeR, pbrInputs, texEnvMap, texEnvMapIrradiance, texBdrfLut);

	vec3 color = vec3(0.0);

	// Calculate lighting contribution from image based lighting source (IBL)
	vec3 diffuseIbl, specularIbl;
	getIBLContribution(pbrInputs, n, pbrInputs.reflection, texEnvMap, texEnvMapIrradiance, texBdrfLut, diffuseIbl, specularIbl);

	if (params.ibl_mode == 0) {
		color = (diffuseIbl + specularIbl) * MeR.r * shadow;
	} else if (params.ibl_mode == 1) {
		color = diffuseIbl * MeR.r * shadow;
	} else if (params.ibl_mode == 2) {
		color = specularIbl * MeR.r * shadow;
	}

	// directional light contribution
	if (params.shading_mode == 0 || params.shading_mode == 1) {
		for (int i = 0; i < params.num_dir_lights; ++i) {
			color += calculatePBRLightContributionDir(pbrInputs, dirLight[i].color, dirLight[i].direction, dirLight[i].intensity) * shadow;
		}
	}

	if (params.shading_mode == 0 || params.shading_mode == 2) {
		for (int i = 0; i < params.num_point_lights; ++i) {
  			color += calculatePBRLightContributionPoint(pbrInputs, worldPos, pointLight[i].color, pointLight[i].position, pointLight[i].intensity);
		}
	}
	
	color += Ke.rgb;
    
	/*
    vec3 viewDir = normalize(worldPos - viewPos);
    vec3 volumetricIntensity = vec3(0.0);
    vec3 accumulatedLight = vec3(0.0);
	float numSamples = 64.0;
	float lightRayLength = 100.0;


    // Sample along the view direction towards the light source
    for (int i = 0; i < numSamples; ++i) { // Number of samples can be adjusted for performance/quality tradeoff
		float weight = float(i) / (numSamples - 1.0);
		float offset = pow(weight, 2.0) * lightRayLength;

        vec3 samplePos = worldPos + viewDir * offset;
        vec4 lightSpacePos = biasMat * sceneData.lightViewProj * vec4(samplePos, 1.0);
        vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

		float occlusion = PCF(5, ndcPos.xy, ndcPos.z, shadowMap);

		accumulatedLight += occlusion * dirLight[0].intensity / (1.0 + params.attenuation  * offset * offset);
    }

    volumetricIntensity = accumulatedLight / numSamples; // Average the accumulated light

	color += volumetricIntensity * params.density;
	*/

	if (params.debugMode == 0) {
		outFragColor = vec4(color, 1.0);

	} else if (params.debugMode == 1) {
		outFragColor = vec4(Kd.rgb, 1.0);
	} else if (params.debugMode == 2) {
		outFragColor = vec4(n, 1.0);
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