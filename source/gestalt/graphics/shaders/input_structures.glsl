
layout(set = 1, binding = 0) uniform samplerCube texEnvMap;
layout(set = 1, binding = 1) uniform samplerCube texEnvMapIrradiance;
layout(set = 1, binding = 2) uniform sampler2D texBdrfLut;

layout(set = 1, binding = 3) uniform sampler2D textures[1280];

layout(std430, set = 1, binding = 4) readonly buffer MaterialConstants {
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
} materialData[256];