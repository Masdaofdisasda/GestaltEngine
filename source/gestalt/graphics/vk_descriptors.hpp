#pragma once

#include <deque>

#include "vk_types.hpp"
#include "EngineConfig.hpp"

#include "Gpu.hpp"

namespace gestalt::graphics {

  struct DescriptorLayoutBuilder final : IDescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> binding_flags;

    DescriptorLayoutBuilder& add_binding(uint32_t binding, VkDescriptorType type,
                                         VkShaderStageFlags shader_stages, bool bindless = false,
                                         uint32_t descriptor_count = 1) override;
    void clear() override;
    VkDescriptorSetLayout build(VkDevice device, bool is_push_descriptor = false) override;
  };

  struct DescriptorWriter final : IDescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                     VkDescriptorType type) override;
    void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                      VkDescriptorType type) override;
    void write_buffer_array(int binding, const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                            VkDescriptorType type, uint32_t arrayElementStart) override;
    void write_image_array(int binding, const std::vector<VkDescriptorImageInfo>& imageInfos,
                           uint32_t arrayElementStart = 0) override;

    void clear() override;
    void update_set(VkDevice device, VkDescriptorSet set) override;
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

  struct DescriptorAllocatorGrowable final : IDescriptorAllocatorGrowable {
    void init(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> poolRatios) override;
    void clear_pools(VkDevice device) override;
    void destroy_pools(VkDevice device) override;

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                             const std::vector<uint32_t>& variableDescriptorCounts = {}) override;

  private:
    VkDescriptorPool get_pool(VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32_t setCount,
                                 std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32 setsPerPool{0};
  };

  struct DescriptorUtilFactory final : public IDescriptorUtilFactory {
    std::unique_ptr<IDescriptorLayoutBuilder> create_descriptor_layout_builder() override;
    std::unique_ptr<IDescriptorWriter> create_descriptor_writer() override;
    std::unique_ptr<IDescriptorAllocatorGrowable> create_descriptor_allocator_growable() override;
  };

  class FrameData {
  public:
    struct FrameResources {
      VkSemaphore swapchain_semaphore, render_semaphore;
      VkFence render_fence;

      VkCommandPool command_pool;
      VkCommandBuffer main_command_buffer;
      DescriptorAllocatorGrowable descriptor_pool;
    };

    void init(VkDevice device, uint32 graphics_queue_family_index);
    void cleanup(VkDevice device);
    void advance_frame();
    [[nodiscard]] uint8 get_current_frame_index() const;
    [[nodiscard]] uint8 get_previous_frame_index() const;
    [[nodiscard]] uint8 get_next_frame_index() const;
    FrameResources& get_current_frame();

  private:
    uint64 frame_number{0};
    // TODO define constants
    std::array<FrameResources, getFramesInFlight()> frames_{};
  };


}  // namespace gestalt::graphics