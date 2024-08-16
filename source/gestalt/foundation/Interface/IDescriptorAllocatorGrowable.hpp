#pragma once

namespace gestalt::foundation {

  class IDescriptorAllocatorGrowable {
  public:
    struct PoolSizeRatio {
      VkDescriptorType type;
      float32 ratio;
    };

    virtual ~IDescriptorAllocatorGrowable() = default;

    virtual void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
        = 0;
    virtual void clear_pools(VkDevice device) = 0;
    virtual void destroy_pools(VkDevice device) = 0;

    virtual VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                                     const std::vector<uint32_t>& variableDescriptorCounts = {})
        = 0;
  };
}  // namespace gestalt