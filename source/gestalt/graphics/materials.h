﻿#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"

struct gltf_metallic_roughness {
  MaterialPipeline opaquePipeline;
  MaterialPipeline transparentPipeline;

  VkDescriptorSetLayout materialLayout;

  struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    // padding, we need it anyway for uniform buffers
    glm::vec4 extra[14];
  };

  struct MaterialResources {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
  };

  DescriptorWriter writer;

  void clear_resources(VkDevice device);

  MaterialInstance write_material(VkDevice device, MaterialPass pass,
                                  const MaterialResources& resources,
                                  DescriptorAllocatorGrowable& descriptorAllocator);
};