#pragma once

#include "common.hpp"
#include "VulkanCore.hpp"
#include "MaterialFlags.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace gestalt::foundation {
  class ImageInstance;

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
        std::shared_ptr<ImageInstance> albedo_image;
        VkSampler albedo_sampler;
        std::shared_ptr<ImageInstance> metal_rough_image;
        VkSampler metal_rough_sampler;
        std::shared_ptr<ImageInstance> normal_image;
        VkSampler normal_sampler;
        std::shared_ptr<ImageInstance> emissive_image;
        VkSampler emissive_sampler;
        std::shared_ptr<ImageInstance> occlusion_image;
        VkSampler occlusion_sampler;
      } textures;
    };
}  // namespace gestalt