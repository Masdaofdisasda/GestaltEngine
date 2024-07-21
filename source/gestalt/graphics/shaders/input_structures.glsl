
layout(set = 1, binding = 1) uniform samplerCube texEnvMap;
layout(set = 1, binding = 2) uniform samplerCube texEnvMapIrradiance;
layout(set = 1, binding = 3) uniform sampler2D texBdrfLut;

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(std430, set = 3, binding = 5) readonly buffer MaterialConstants {
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
} materialData[];