#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"

struct gltf_material {

  struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    //glm::vec3 emissiveFactor;
    //float normalScale;
    //float occlusionStrength;
    // Additional padding or data as needed
    glm::vec4 extra[14];  // Adjust the padding based on your new data
  };

  struct MaterialResources {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    AllocatedImage normalImage;
    VkSampler normalSampler;
    AllocatedImage emissiveImage;
    VkSampler emissiveSampler;
    AllocatedImage occlusionImage;
    VkSampler occlusionSampler;
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
  };
};