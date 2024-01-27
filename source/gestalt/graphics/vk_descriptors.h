#pragma once

#include <vk_types.h>

#include "vk_deletion_service.h"
#include "vk_gpu.h"

struct descriptor_layout_builder {
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  descriptor_layout_builder& add_binding(uint32_t binding, VkDescriptorType type,
                                       VkShaderStageFlags shader_stages);
  void clear();
  VkDescriptorSetLayout build(VkDevice device);
};

struct descriptor_writer {
  std::deque<VkDescriptorImageInfo> imageInfos;
  std::deque<VkDescriptorBufferInfo> bufferInfos;
  std::vector<VkWriteDescriptorSet> writes;

  void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                   VkDescriptorType type);
  void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                    VkDescriptorType type);

  void clear();
  void update_set(VkDevice device, VkDescriptorSet set);
};

struct descriptor_allocator {
  struct pool_size_ratio {
    VkDescriptorType type;
    float ratio;
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

  VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
  VkDescriptorPool get_pool(VkDevice device);
  VkDescriptorPool create_pool(VkDevice device, uint32_t setCount,
                               std::span<PoolSizeRatio> poolRatios);

  std::vector<PoolSizeRatio> ratios;
  std::vector<VkDescriptorPool> fullPools;
  std::vector<VkDescriptorPool> readyPools;
  uint32_t setsPerPool;
};

struct frame_data {
  VkSemaphore swapchain_semaphore, render_semaphore;
  VkFence render_fence;

  VkCommandPool command_pool;
  VkCommandBuffer main_command_buffer;

  vk_deletion_service deletion_queue;
  DescriptorAllocatorGrowable frame_descriptors;
};

class vk_descriptor_manager {
  vk_gpu gpu_;
  vk_deletion_service deletion_service_;

public:
  VkDescriptorSetLayout gpu_scene_data_descriptor_layout;

  void init(const vk_gpu& gpu, std::vector<frame_data>& frames);
  void cleanup();
};