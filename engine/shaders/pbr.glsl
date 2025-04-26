
struct MaterialInfo 
{

	float NdotL;                  // cos angle between normal and light direction
	float NdotV;                  // cos angle between normal and view direction
	float NdotH;                  // cos angle between normal and half vector
	float LdotH;                  // cos angle between light direction and half vector
	float VdotH;                  // cos angle between view direction and half vector
	vec3 reflectance0;            // full reflectance color (normal incidence angle)
	vec3 reflectance90;           // reflectance color at grazing angle
	vec3 f_diffuse;            // color contribution from diffuse lighting
	vec3 f_specular;           // color contribution from specular lighting
	vec3 f_emissive;           // color contribution from specular lighting
    vec3 f_diffuse_ibl;
    vec3 f_specular_ibl;
	vec3 n;							// normal at surface point
	vec3 t;							// TODO
	vec3 b;							// TODO
	vec3 v;							// vector from surface point to camera
	vec3 reflection;

    float ior;
    float perceptualRoughness;      // roughness value, as authored by the model creator (input to shader)
    vec3 f0;                        // full reflectance color (n incidence angle)

    float alphaRoughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 c_diff;

    vec3 f90;                       // reflectance color at grazing angle
    float metallic;

    vec3 baseColor;

    float sheenRoughnessFactor;
    vec3 sheenColorFactor;

    vec3 clearcoatF0;
    vec3 clearcoatF90;
    float clearcoatFactor;
    vec3 clearcoatNormal;
    float clearcoatRoughness;

    // KHR_materials_specular 
    float specularWeight; // product of specularFactor and specularTexture.a

    float transmissionFactor;

    float thickness;
    vec3 attenuationColor;
    float attenuationDistance;

    // KHR_materials_iridescence
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceThickness;

    // KHR_materials_anisotropy
    vec3 anisotropicT;
    vec3 anisotropicB;
    float anisotropyStrength;

    // KHR_materials_dispersion
    float dispersion;
};

const float M_PI = 3.141592653589793;

vec3 getIBLRadianceGGX(MaterialInfo materialInfo, sampler2D texBdrfLut, samplerCube texEnvMap)
{
    //float lod = 0; //roughness * float(u_MipCount - 1); Todo
    float mipmapLevel = 9; //textureQueryLod(texEnvMap, materialInfo.reflection).x;
    float lod = materialInfo.perceptualRoughness * mipmapLevel-1;

    vec2 brdfSamplePoint = clamp(vec2(materialInfo.NdotV, materialInfo.perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(texBdrfLut, brdfSamplePoint).rg;
    vec4 specularSample = textureLod(texEnvMap, materialInfo.reflection, lod);

    vec3 specularLight = specularSample.rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - materialInfo.perceptualRoughness), materialInfo.f0) - materialInfo.f0;
    vec3 k_S = materialInfo.f0 + Fr * pow(1.0 - materialInfo.NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return materialInfo.specularWeight * specularLight * FssEss;
}

// specularWeight is introduced with KHR_materials_specular
vec3 getIBLRadianceLambertian(MaterialInfo materialInfo, sampler2D texBdrfLut, samplerCube texEnvMapIrradiance)
{

    vec3 diffuseIrradiance = texture(texEnvMapIrradiance, materialInfo.n).rgb;
    vec2 brdfSamplePoint = clamp(vec2(materialInfo.NdotV, materialInfo.perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(texBdrfLut, brdfSamplePoint).rg;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera

    vec3 Fr = max(vec3(1.0 - materialInfo.perceptualRoughness), materialInfo.f0) - materialInfo.f0;
    vec3 k_S = materialInfo.f0 + Fr * pow(1.0 - materialInfo.NdotV, 5.0);
    vec3 FssEss = materialInfo.specularWeight * k_S * f_ab.x + f_ab.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = materialInfo.specularWeight * (materialInfo.f0 + (1.0 - materialInfo.f0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = materialInfo.c_diff * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)
    
    return diffuseIrradiance * k_D;
}

void calculatePBRInputsMetallicRoughness( vec4 albedo, vec3 normal, vec3 cameraPos, vec3 worldPos, vec4 mrSample, out MaterialInfo materialInfo)
{
	materialInfo.baseColor = albedo.rgb;
	// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
	// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
	materialInfo.perceptualRoughness = mrSample.g;
	materialInfo.metallic = mrSample.b;

	materialInfo.ior = 1.5;
    materialInfo.f0 = mix(vec3(0.04), materialInfo.baseColor.rgb, materialInfo.metallic);;
    materialInfo.specularWeight = 1.0;

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;
	
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);

	materialInfo.f_diffuse = vec3(0.0);
	materialInfo.f_specular = vec3(0.0);
    materialInfo.f_diffuse_ibl = vec3(0.0);
    materialInfo.f_specular_ibl = vec3(0.0);
    materialInfo.f_emissive = vec3(0.0);

	// The albedo may be defined from a base texture or a flat color
    materialInfo.c_diff = mix(materialInfo.baseColor.rgb,  vec3(0), materialInfo.metallic);

	vec3 specularColor = mix(materialInfo.f0, materialInfo.baseColor.rgb, materialInfo.metallic);

	// Compute reflectance.
	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);

	materialInfo.n = normalize(normal);					// normal at surface point
    //vec3 t = normalInfo.t;
    //vec3 b = normalInfo.b;
	materialInfo.v = normalize(cameraPos - worldPos);	// Vector from surface point to camera
	materialInfo.NdotV = clamp(abs(dot(materialInfo.n, materialInfo.v)), 0.001, 1.0);
    //float TdotV = clampedDot(t, v);
    //float BdotV = clampedDot(b, v);
	materialInfo.reflectance0 = specularColor.rgb;
	materialInfo.reflectance90 = vec3(1.0, 1.0, 1.0) * reflectance90;
	materialInfo.reflection = -normalize(reflect(materialInfo.v, materialInfo.n));
}


// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float F_Schlick(float f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float F_Schlick(float f0, float VdotH)
{
    float f90 = 1.0; //clamp(50.0 * f0, 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 F_Schlick(vec3 f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

vec3 F_Schlick(vec3 f0, float VdotH)
{
    float f90 = 1.0; //clamp(dot(f0, vec3(50.0 * 0.33)), 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 Schlick_to_F0(vec3 f, vec3 f90, float VdotH) {
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

float Schlick_to_F0(float f, float f90, float VdotH) {
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

vec3 Schlick_to_F0(vec3 f, float VdotH) {
    return Schlick_to_F0(f, vec3(1.0), VdotH);
}

float Schlick_to_F0(float f, float VdotH) {
    return Schlick_to_F0(f, 1.0, VdotH);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}


// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, float VdotH)
{
    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_specularGGX(vec3 f0, vec3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return specularWeight * F * Vis * D;
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float getRangeAttenuation(float range, float distance)
{
    if (range <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / pow(distance, 2.0);
    }
    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float getSpotAttenuation(vec3 pointToLight, vec3 spotDirection, float outerConeCos, float innerConeCos)
{
    float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));
    if (actualCos > outerConeCos)
    {
        if (actualCos < innerConeCos)
        {
            float angularAttenuation = (actualCos - outerConeCos) / (innerConeCos - outerConeCos);
            return angularAttenuation * angularAttenuation;
        }
        return 1.0;
    }
    return 0.0;
}

vec3 getPointLighIntensity(vec3 lightColor, float lightIntensity, float range, vec3 pointToLight)
{
    float rangeAttenuation = getRangeAttenuation(range, length(pointToLight));

    return rangeAttenuation * lightIntensity * lightColor;
}

vec3 getDirectionalLighIntensity(vec3 lightColor, float lightIntensity)
{
    return lightIntensity * lightColor;
}

vec3 getSpotLighIntensity(vec3 lightColor, float lightIntensity, vec3 direction, vec3 pointToLight, float range, float outerConeCos, float innerConeCos)
{
    float rangeAttenuation = getRangeAttenuation(range, length(pointToLight));
    float spotAttenuation  = getSpotAttenuation(pointToLight, direction, outerConeCos, innerConeCos);

    return rangeAttenuation * spotAttenuation * lightIntensity * lightColor;
}