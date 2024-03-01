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

layout( push_constant ) uniform constants
{	
	mat4 invViewProj;
	vec2 screenSize;
	float density;
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

	float bias = calculateDynamicBias(n, sceneData.light_direction);
	float shadow = shadowFactor(lightSpacePos, shadowMap, bias).r;

	PBRInfo pbrInputs;
	
	vec3 color = vec3(0.0);

	// IBL contribution
	color = calculatePBRInputsMetallicRoughness(Kd, n, viewPos.xyz, worldPos, MeR, pbrInputs, texEnvMap, texEnvMapIrradiance, texBdrfLut) * MeR.r;

	// directional light contribution
	color *= calculatePBRLightContributionDir(pbrInputs, sceneData.light_direction, sceneData.light_intensity) * shadow;

  	color += calculatePBRLightContributionPoint(pbrInputs, worldPos, sceneData.light_intensity);

	color += Ke.rgb;
    
    vec3 viewDir = normalize(worldPos - viewPos);
    float volumetricIntensity = 0.0;
    vec3 accumulatedLight = vec3(0.0);
	float numSamples = 16.0;
	float lightRayLength = 100.0;


    // Sample along the view direction towards the light source
    for (int i = 0; i < numSamples; ++i) { // Number of samples can be adjusted for performance/quality tradeoff
		float weight = float(i) / (numSamples - 1.0);
		float offset = pow(weight, 2.0) * lightRayLength;

        vec3 samplePos = worldPos + viewDir * offset;
        vec4 lightSpacePos = biasMat * sceneData.lightViewProj * vec4(samplePos, 1.0);
        vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;
        ndcPos = ndcPos * 0.5 + 0.5; // Transform to [0, 1] range

        float shadow = texture(shadowMap, ndcPos.xy).r;
        float occlusion = shadow >= ndcPos.z ? 0.0 : 1.0; // Basic shadow test

		accumulatedLight += occlusion * sceneData.light_intensity / (1.0 + params.density * offset * offset);
    }

    volumetricIntensity = length(accumulatedLight) / numSamples; // Average the accumulated light

	color += volumetricIntensity * params.density;

    outFragColor = vec4(color, 1.0);
	
	//outFragColor = vec4(normalize(worldPos) * 0.5 + 0.5, 1.0);

	float normalizedDepth = depth * 2.0 - 1.0; // Assuming depth is in [0,1], map it to [-1,1]
	//outFragColor = vec4(vec3(normalizedDepth), 1.0); // Visualize depth as grayscale color

	vec3 normalizedLightSpacePos = lightSpacePos.xyz / lightSpacePos.w;
	//outFragColor = vec4(normalize(normalizedLightSpacePos) * 0.5 + 0.5, 1.0);



}