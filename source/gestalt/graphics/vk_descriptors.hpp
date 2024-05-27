#pragma once

#include "vk_types.hpp"

#include "DeletionService.hpp"
#include "Gpu.hpp"

namespace gestalt::graphics {
    
  struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> binding_flags;

    DescriptorLayoutBuilder& add_binding(uint32_t binding, VkDescriptorType type,
                                         VkShaderStageFlags shader_stages, bool bindless = false,
                                         uint32_t descriptor_count = 1);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, bool is_push_descriptor = false);
  };

  struct descriptor_writer {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                     VkDescriptorType type);
    void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                      VkDescriptorType type);
    void write_buffer_array(int binding, const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                            VkDescriptorType type, uint32_t arrayElementStart);
    void write_image_array(int binding, const std::vector<VkDescriptorImageInfo>& imageInfos,
                           uint32_t arrayElementStart = 0);

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
  };

  struct DescriptorAllocator {
    struct pool_size_ratio {
      VkDescriptorType type;
      float32 ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32_t maxSets, std::span<pool_size_ratio> poolRatios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
  };

  struct DescriptorAllocatorGrowable {
    struct PoolSizeRatio {
      VkDescriptorType type;
      float ratio;
    };

    void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void clear_pools(VkDevice device);
    void destroy_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                             const std::vector<uint32_t>& variableDescriptorCounts = {});

  private:
    VkDescriptorPool get_pool(VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32_t setCount,
                                 std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32 setsPerPool;
  };

  struct FrameData {
    VkSemaphore swapchain_semaphore, render_semaphore;
    VkFence render_fence;

    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;
    DescriptorAllocatorGrowable descriptor_pool;
  };
}  // namespace gestalt