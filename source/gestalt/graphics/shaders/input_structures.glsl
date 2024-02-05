
layout(set = 1, binding = 1) uniform samplerCube texEnvMap;
layout(set = 1, binding = 2) uniform samplerCube texEnvMapIrradiance;
//layout(set = 1, binding = 3) uniform samplerCube texBdrfLut;

layout(set = 2, binding = 4) uniform sampler2D textures[];

layout(std430, binding = 5) readonly buffer MaterialConstants {
    int albedo_tex_index;
    int metal_rough_tex_index;
    int normal_tex_index;
    int emissive_tex_index;
    int occlusion_tex_index;

    int texture_flags;

	vec4 colorFactors;
	vec2 metal_rough_factors;
} materialData[];
