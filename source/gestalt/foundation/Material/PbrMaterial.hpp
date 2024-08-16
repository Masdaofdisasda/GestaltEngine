#pragma once
#include "MaterialFlags.hpp"

namespace gestalt::foundation {

    struct PbrMaterial {
      bool double_sided{false};
      bool transparent{false};

      struct alignas(64) PbrConstants {
        uint16 albedo_tex_index = kUnusedTexture;
        uint16 metal_rough_tex_index = kUnusedTexture;
        uint16 normal_tex_index = kUnusedTexture;
        uint16 emissive_tex_index = kUnusedTexture;
        uint16 occlusion_tex_index = kUnusedTexture;
        uint32 flags{0};

        glm::vec4 albedo_color{1.f};
        glm::vec2 metal_rough_factor = {.0f, 0.f};  // roughness, metallic
        float32 occlusionStrength{1.f};
        float32 alpha_cutoff{0.f};
        glm::vec3 emissiveColor{0.f};
        float32 emissiveStrength{1.0};
      } constants;

      struct PbrTextures {
        TextureHandle albedo_image;
        VkSampler albedo_sampler;
        TextureHandle metal_rough_image;
        VkSampler metal_rough_sampler;
        TextureHandle normal_image;
        VkSampler normal_sampler;
        TextureHandle emissive_image;
        VkSampler emissive_sampler;
        TextureHandle occlusion_image;
        VkSampler occlusion_sampler;
      } textures;
    };
}  // namespace gestalt