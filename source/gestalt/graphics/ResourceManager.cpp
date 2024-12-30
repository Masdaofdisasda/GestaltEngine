#include "ResourceManager.hpp"

#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"


namespace gestalt::graphics {

    void ResourceManager::init(IGpu* gpu,
                               Repository* repository) {
      gpu_ = gpu;
      repository_ = repository;
      generate_samplers();
    }

    VkSampler ResourceManager::create_sampler(const VkSamplerCreateInfo& sampler_create_info) const {
      VkSampler new_sampler;
      vkCreateSampler(gpu_->getDevice(), &sampler_create_info, nullptr, &new_sampler);
      return new_sampler;
    }

  void ResourceManager::generate_samplers() const {
      std::vector<SamplerConfig> permutations;
      std::vector filters = {VK_FILTER_NEAREST, VK_FILTER_LINEAR};
      std::vector mipmapModes
          = {VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR};
      std::vector addressModes
          = {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
             VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};
      std::vector anisotropies = {16.0f};
      std::vector anisotropyEnabled = {VK_FALSE, VK_TRUE};
      std::vector borderColors = {VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK};

      for (auto magFilter : filters) {
        for (auto minFilter : filters) {
          for (auto mipmapMode : mipmapModes) {
            for (auto addressModeU : addressModes) {
              for (auto addressModeV : addressModes) {
                for (auto addressModeW : addressModes) {
                  for (auto maxAnisotropy : anisotropies) {
                    for (auto anisotropyEnable : anisotropyEnabled) {
                      SamplerConfig config
                          = {magFilter,    minFilter,    mipmapMode,    addressModeU,
                             addressModeV, addressModeW, maxAnisotropy, anisotropyEnable};
                      permutations.push_back(config);
                    }
                  }
                }
              }
            }
          }
        }
      }

        VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      for (const auto config : permutations) {
        sampler_info.magFilter = config.magFilter;
        sampler_info.minFilter = config.minFilter;
        sampler_info.mipmapMode = config.mipmapMode;
        sampler_info.addressModeU = config.addressModeU;
        sampler_info.addressModeV = config.addressModeV;
        sampler_info.addressModeW = config.addressModeW;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = config.anisotropyEnable;
        sampler_info.maxAnisotropy = config.maxAnisotropy;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        VkSampler new_sampler;
        vkCreateSampler(gpu_->getDevice(), &sampler_info, nullptr, &new_sampler);
        repository_->sampler_cache[config] = new_sampler;
      }

    }

}  // namespace gestalt