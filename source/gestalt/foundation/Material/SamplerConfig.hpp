﻿#pragma once
#include "common.hpp"

namespace gestalt::foundation {

  struct SamplerConfig {
    VkFilter magFilter{VK_FILTER_LINEAR};
    VkFilter minFilter{VK_FILTER_LINEAR};
    VkSamplerMipmapMode mipmapMode{VK_SAMPLER_MIPMAP_MODE_LINEAR};
    VkSamplerAddressMode addressModeU{VK_SAMPLER_ADDRESS_MODE_REPEAT};
    VkSamplerAddressMode addressModeV{VK_SAMPLER_ADDRESS_MODE_REPEAT};
    VkSamplerAddressMode addressModeW{VK_SAMPLER_ADDRESS_MODE_REPEAT};
    float maxAnisotropy{16.f};
    VkBool32 anisotropyEnable{VK_TRUE};

    bool operator==(const SamplerConfig& other) const {
      return magFilter == other.magFilter && minFilter == other.minFilter
             && mipmapMode == other.mipmapMode && addressModeU == other.addressModeU
             && addressModeV == other.addressModeV && addressModeW == other.addressModeW
             && maxAnisotropy == other.maxAnisotropy && anisotropyEnable == other.anisotropyEnable;
    }
  };
  struct SamplerConfigHash {
    std::size_t operator()(const SamplerConfig& config) const {
      return std::hash<int>()(static_cast<int>(config.magFilter))
             ^ std::hash<int>()(static_cast<int>(config.minFilter))
             ^ std::hash<int>()(static_cast<int>(config.mipmapMode))
             ^ std::hash<int>()(static_cast<int>(config.addressModeU))
             ^ std::hash<int>()(static_cast<int>(config.addressModeV))
             ^ std::hash<int>()(static_cast<int>(config.addressModeW))
             ^ std::hash<float>()(config.maxAnisotropy)
             ^ std::hash<int>()(static_cast<int>(config.anisotropyEnable));
    }
  };
}  // namespace gestalt