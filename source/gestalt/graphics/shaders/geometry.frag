#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#include "per_frame_structs.glsl"
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inPosition;
layout (location = 2) in vec2 inUV;
layout (location = 3) flat in int inMaterialIndex;
layout (location = 4) flat in int inMaterialConst;

layout (location = 0) out vec4 outFragColor;

struct PBRInfo
{
	float NdotL;                  // cos angle between normal and light direction
	float NdotV;                  // cos angle between normal and view direction
	float NdotH;                  // cos angle between normal and half vector
	float LdotH;                  // cos angle between light direction and half vector
	float VdotH;                  // cos angle between view direction and half vector
	float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
	vec3 reflectance0;            // full reflectance color (normal incidence angle)
	vec3 reflectance90;           // reflectance color at grazing angle
	float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
	vec3 diffuseColor;            // color contribution from diffuse lighting
	vec3 specularColor;           // color contribution from specular lighting
	vec3 n;							// normal at surface point
	vec3 v;							// vector from surface point to camera
};

const float M_PI = 3.141592653589793;

vec3 EnvBRDFApprox( //Todo
 vec3 specularColor, float roughness, float NoV )
{
  const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
  const vec4 c1 = vec4( 1, 0.0425, 1.04, -0.04);
  vec4 r = roughness * c0 + c1;
  float a004 =
 min( r.x * r.x, exp2(-9.28 * NoV) ) * r.x + r.y;
  vec2 AB = vec2( -1.04, 1.04 ) * a004 + r.zw;
  return specularColor * AB.x + AB.y;
}

vec3 sRGBToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + vec3(0.055)) / vec3(1.055), vec3(2.4)), step(vec3(0.04045), color));
}

vec3 linearTosRGB(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - vec3(0.055), step(vec3(0.0031308), color));
}


// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 getIBLContribution(PBRInfo pbrInputs, vec3 n, vec3 reflection)
{
	float mipCount = float(textureQueryLevels(texEnvMap));
	float lod = pbrInputs.perceptualRoughness * mipCount;
	// retrieve a scale and bias to F0. See [1], Figure 3
	vec2 brdfSamplePoint = clamp(vec2(pbrInputs.NdotV, 1.0-pbrInputs.perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
	//vec3 brdf = textureLod(brdfLutTex, brdfSamplePoint, 0).rgb;
	vec3 cm = vec3(1.0, 1.0, 1.0);
	// HDR envmaps are already linear
	vec3 diffuseLight = texture(texEnvMapIrradiance, n.xyz * cm).rgb;
	vec3 specularLight = textureLod(texEnvMap, reflection.xyz * cm, lod).rgb;

	vec3 diffuse = diffuseLight * pbrInputs.diffuseColor;
	//TODO vec3 specular = specularLight * (pbrInputs.specularColor * brdf.x + brdf.y);
	vec3 specular = specularLight * EnvBRDFApprox(pbrInputs.specularColor, pbrInputs.perceptualRoughness, pbrInputs.NdotV);

	return diffuse + specular ;
}

vec3 calculatePBRInputsMetallicRoughness( vec4 albedo, vec3 normal, vec3 cameraPos, vec3 worldPos, vec4 mrSample, out PBRInfo pbrInputs )
{
	float perceptualRoughness = 1.0;
	float metallic = 1.0;

	// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
	// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
	perceptualRoughness = mrSample.g * perceptualRoughness;
	metallic = mrSample.b * metallic;

	const float c_MinRoughness = 0.04;

	perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
	metallic = clamp(metallic, 0.0, 1.0);
	// Roughness is authored as perceptual roughness; as is convention,
	// convert to material roughness by squaring the perceptual roughness [2].
	float alphaRoughness = perceptualRoughness * perceptualRoughness;

	// The albedo may be defined from a base texture or a flat color
	vec4 baseColor = albedo;

	vec3 f0 = vec3(0.04);
	vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
	diffuseColor *= 1.0-metallic;
	vec3 specularColor = mix(f0, baseColor.rgb, metallic);

	// Compute reflectance.
	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	vec3 n = normalize(normal);					// normal at surface point
	vec3 v = normalize(cameraPos - worldPos);	// Vector from surface point to camera
	vec3 reflection = -normalize(reflect(v, n));

	pbrInputs.NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	pbrInputs.perceptualRoughness = perceptualRoughness;
	pbrInputs.reflectance0 = specularEnvironmentR0;
	pbrInputs.reflectance90 = specularEnvironmentR90;
	pbrInputs.alphaRoughness = alphaRoughness;
	pbrInputs.diffuseColor = diffuseColor;
	pbrInputs.specularColor = specularColor;
	pbrInputs.n = n;
	pbrInputs.v = v;

	// Calculate lighting contribution from image based lighting source (IBL)
	vec3 color = getIBLContribution(pbrInputs, n, reflection);

	return color;
}

// Disney Implementation of diffuse from Physically-Based Shading at Disney by Brent Burley. See Section 5.3.
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
vec3 diffuseBurley(PBRInfo pbrInputs)
{
	float f90 = 2.0 * pbrInputs.LdotH * pbrInputs.LdotH * pbrInputs.alphaRoughness - 0.5;

	return (pbrInputs.diffuseColor / M_PI) * (1.0 + f90 * pow((1.0 - pbrInputs.NdotL), 5.0)) * (1.0 + f90 * pow((1.0 - pbrInputs.NdotV), 5.0));
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
	return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
	float NdotL = pbrInputs.NdotL;
	float NdotV = pbrInputs.NdotV;
	float rSqr = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;

	float attenuationL = 2.0 * NdotL / (NdotL + sqrt(rSqr + (1.0 - rSqr) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(rSqr + (1.0 - rSqr) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
	float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
	float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
	return roughnessSq / (M_PI * f * f);
}

vec3 calculatePBRLightContributionDir( inout PBRInfo pbrInputs)
{
	vec3 n = pbrInputs.n;
	vec3 v = pbrInputs.v;
	vec3 l = normalize(sceneData.sunlightDirection.xyz);	// Vector from surface point to light
	vec3 h = normalize(l+v);					// Half vector between both l and v

	float NdotV = pbrInputs.NdotV;
	float NdotL = clamp(dot(n, l), 0.001, 1.0);
	float NdotH = clamp(dot(n, h), 0.0, 1.0);
	float LdotH = clamp(dot(l, h), 0.0, 1.0);
	float VdotH = clamp(dot(v, h), 0.0, 1.0);

	pbrInputs.NdotL = NdotL;
	pbrInputs.NdotH = NdotH;
	pbrInputs.LdotH = LdotH;
	pbrInputs.VdotH = VdotH;

	// Calculate the shading terms for the microfacet specular shading model
	vec3 F = specularReflection(pbrInputs);
	float G = geometricOcclusion(pbrInputs);
	float D = microfacetDistribution(pbrInputs);

	// Calculation of analytical lighting contribution
	vec3 diffuseContrib = (1.0 - F) * diffuseBurley(pbrInputs);
	vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	vec3 color = NdotL * sceneData.sunlightDirection.w * 3.0f * (diffuseContrib + specContrib);

	return color;
}

vec3 calculatePBRLightContributionPoint( inout PBRInfo pbrInputs)
{
	vec3 lightPos = vec3(0.0, 0.0, 0.0);

	vec3 n = pbrInputs.n;
	vec3 v = pbrInputs.v;
	vec3 l = normalize(lightPos - inPosition);	// Vector from surface point to light
	vec3 h = normalize(l+v);							// Half vector between both l and v

	float NdotV = pbrInputs.NdotV;
	float NdotL = clamp(dot(n, l), 0.001, 1.0);
	float NdotH = clamp(dot(n, h), 0.0, 1.0);
	float LdotH = clamp(dot(l, h), 0.0, 1.0);
	float VdotH = clamp(dot(v, h), 0.0, 1.0);

	pbrInputs.NdotL = NdotL;
	pbrInputs.NdotH = NdotH;
	pbrInputs.LdotH = LdotH;
	pbrInputs.VdotH = VdotH;

	// Calculate the shading terms for the microfacet specular shading model
	vec3 F = specularReflection(pbrInputs);
	float G = geometricOcclusion(pbrInputs);
	float D = microfacetDistribution(pbrInputs);

	// Calculation of analytical lighting contribution
	vec3 diffuseContrib = (1.0 - F) * diffuseBurley(pbrInputs);
	vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float distance = length(vec3(lightPos) - inPosition);
    float attenuation = 1.0 / (distance * distance);
	vec3 color = NdotL * sceneData.sunlightDirection.w * attenuation * (diffuseContrib + specContrib);

	return color;
}

// http://www.thetenthplanet.de/archives/1180
mat3 cotangentFrame( vec3 N, vec3 p, vec2 uv )
{
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx( p );
	vec3 dp2 = dFdy( p );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );

	// solve the linear system
	vec3 dp2perp = cross( dp2, N );
	vec3 dp1perp = cross( N, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );

	// calculate handedness of the resulting cotangent frame
	float w = (dot(cross(N, T), B) < 0.0) ? -1.0 : 1.0;

	// adjust tangent if needed
	T = T * w;

	return mat3( T * invmax, B * invmax, N );
}
vec3 perturbNormal(vec3 n, vec3 v, vec3 normalSample, vec2 uv)
{
	vec3 map = normalize( 2.0 * normalSample - vec3(1.0) );
	mat3 TBN = cotangentFrame(n, v, uv);
	return normalize(TBN * map);
}

void main() {

	uint albedoIndex =			inMaterialIndex;
	uint metalicRoughIndex =	inMaterialIndex + 1;
	uint normalIndex =			inMaterialIndex + 2;
	uint emissiveIndex =		inMaterialIndex + 3;
	uint occlusionIndex =		inMaterialIndex + 4;

    vec2 UV = inUV;

    vec4 Kd = texture(nonuniformEXT(textures[albedoIndex]), UV);
	Kd.rgb = sRGBToLinear(Kd.rgb);
	float alphaCutoff = materialData[nonuniformEXT(inMaterialConst)].alpha_cutoff;
    if (Kd.a < alphaCutoff) {
        discard;
    }

    vec3 normal_sample = texture(nonuniformEXT(textures[normalIndex]), UV).rgb;
	vec3 viewPos = -normalize(vec3(sceneData.view[0][2], sceneData.view[1][2], sceneData.view[2][2]));
    vec3 n = perturbNormal(normalize(inNormal), normalize(viewPos - inPosition), normal_sample, UV);

	vec4 Ke = texture(nonuniformEXT(textures[emissiveIndex]), UV);
	Ke.rgb = sRGBToLinear(Ke.rgb);

	float Kao = texture(nonuniformEXT(textures[occlusionIndex]), UV).r;

	vec4 MeR = texture(nonuniformEXT(textures[metalicRoughIndex]), UV);

	PBRInfo pbrInputs;
	
	vec3 color = vec3(0.0);

	// IBL contribution
	color = calculatePBRInputsMetallicRoughness(Kd, n, viewPos.xyz, inPosition, MeR, pbrInputs);

	// directional light contribution
	color *= calculatePBRLightContributionDir(pbrInputs);

	
  	color += calculatePBRLightContributionPoint(pbrInputs);

	color = color * (Kao.r < 0.01 ? 1.0 : Kao);
	color = pow(Ke.rgb + color, vec3(1.0/2.2) ) ;

    outFragColor = vec4(color, 1.0);
}