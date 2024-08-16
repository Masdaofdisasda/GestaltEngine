#pragma once

namespace gestalt::foundation {

  struct DescriptorUpdate {
    VkDescriptorType type;
    size_t descriptorSize;
    uint32 binding;
    uint32 descriptorIndex;
    VkDescriptorAddressInfoEXT addr_info; // only relevant for buffer resources
    VkDescriptorImageInfo image_info; // only relevant for image resources
  };
}  // namespace gestalt