#include "vk_descriptors.hpp"

#include "vk_initializers.hpp"
#include "RenderConfig.hpp"
#include "ValidationCallback.hpp"
#include <fmt/core.h>

namespace gestalt::graphics {
  DescriptorLayoutBuilder& DescriptorLayoutBuilder::add_binding(uint32_t binding,
                                                                VkDescriptorType type,
                                                                VkShaderStageFlags shader_stages,
                                                                bool bindless,
                                                                uint32_t descriptor_count) {
    uint32_t descriptorCount = bindless ? 4096 : descriptor_count;  // TODO rethink this design

    bindings.emplace_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = descriptorCount,
        .stageFlags = shader_stages,
    });

    if (bindless) {
      binding_flags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT);
    } else {
      binding_flags.push_back(0);  // Default flag for regular bindings
    }

    return *this;
  }

  void DescriptorLayoutBuilder::clear() {
    bindings.clear();
    binding_flags.clear();
  }

  VkDescriptorSetLayout DescriptorLayoutBuilder::build(
      VkDevice device, VkDescriptorSetLayoutCreateFlags flags) {
    VkDescriptorSetLayoutCreateInfo info
        = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
           .pNext = nullptr,
           .flags = flags | VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
           .bindingCount = static_cast<uint32_t>(bindings.size()),
           .pBindings = bindings.data()};

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsInfo
        = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
           .bindingCount = static_cast<uint32_t>(binding_flags.size()),
           .pBindingFlags = binding_flags.data()};

    if (!binding_flags.empty()) {
      info.pNext = &bindingFlagsInfo;  // Chain the binding flags info
    }

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
  }

  void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                                      VkDescriptorType type) {
    VkDescriptorBufferInfo& info = bufferInfos.emplace_back(
        VkDescriptorBufferInfo{.buffer = buffer, .offset = offset, .range = size});

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;  // left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    writes.push_back(write);
  }

  void DescriptorWriter::write_buffer_array(int binding,
                                            const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                                            VkDescriptorType type, uint32_t arrayElementStart) {
    for (const auto& bufferInfo : bufferInfos) {
      this->bufferInfos.push_back(bufferInfo);
    }

    // Prepare a write descriptor set for an array of buffers
    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;  // Will be set in update_set()
    write.descriptorCount = static_cast<uint32_t>(bufferInfos.size());
    write.descriptorType = type;
    write.pBufferInfo = &this->bufferInfos.back() - (bufferInfos.size() - 1);
    write.dstArrayElement = arrayElementStart;

    // Add to the list of writes
    writes.push_back(write);
  }

  void DescriptorWriter::write_image_array(
      int binding, const std::array<VkDescriptorImageInfo, 5>& image_infos,
                                           uint32_t arrayElementStart) {
    for (const auto& imageInfo : image_infos) {
      imageInfos.push_back(imageInfo);
    }

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;  // Will be set in update_set()
    write.descriptorCount = image_infos.size();
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfos.back() - (image_infos.size() - 1);
    write.dstArrayElement = arrayElementStart;

    writes.push_back(write);
  }

  void DescriptorWriter::clear() {
    imageInfos.clear();
    writes.clear();
    bufferInfos.clear();

    writes.reserve(getMaxMaterials());
  }

  void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set) {
    assert(set != VK_NULL_HANDLE && "Descriptor set is VK_NULL_HANDLE");

    for (VkWriteDescriptorSet& write : writes) {
      write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
  }

  void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler,
                                     VkImageLayout layout, VkDescriptorType type) {
    VkDescriptorImageInfo& info = imageInfos.emplace_back(
        VkDescriptorImageInfo{.sampler = sampler, .imageView = image, .imageLayout = layout});

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;  // left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;

    writes.push_back(write);
  }

  void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets,
                                      std::span<pool_size_ratio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto [type, ratio] : poolRatios) {
      poolSizes.push_back(VkDescriptorPoolSize{.type = type,
                                               .descriptorCount = static_cast<uint32_t>(ratio * maxSets)});
    }

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool_info.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
  }

  void DescriptorAllocator::clear_descriptors(VkDevice device) const {
    vkResetDescriptorPool(device, pool, 0);
  }

  void DescriptorAllocator::destroy_pool(VkDevice device) const {
    vkDestroyDescriptorPool(device, pool, nullptr);
  }

  VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) const {
    VkDescriptorSetAllocateInfo allocInfo
        = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
  }

  VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device) {
    VkDescriptorPool newPool;
    if (!readyPools.empty()) {
      newPool = readyPools.back();
      readyPools.pop_back();
    } else {
      // need to create a new pool
      newPool = create_pool(device, setsPerPool, ratios);

      setsPerPool = setsPerPool * 1.5;
      if (setsPerPool > 4092) {
        fmt::println("Descriptor pool size exceeded 4092. Resetting to 4092");
        setsPerPool = 4092;
      }
    }

    return newPool;
  }

  VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount,
                                                            std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto [type, ratio] : poolRatios) {
      poolSizes.push_back(VkDescriptorPoolSize{
          .type = type, .descriptorCount = static_cast<uint32_t>(ratio * setCount)});
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool_info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool));
    return newPool;
  }

  void FrameData::init(VkDevice device, uint32 graphics_queue_family_index, FrameProvider* frame) {
      frame_ = frame;

    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        graphics_queue_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBufferAllocateInfo cmdAllocInfo
        = vkinit::command_buffer_allocate_info(VK_NULL_HANDLE, 1);
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();


    for (auto& frame : frames_) {
      // Initialize Command Pool and Command Buffer
      VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frame.command_pool));
      cmdAllocInfo.commandPool = frame.command_pool;
      VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frame.main_command_buffer));

      // Initialize Fences and Semaphores
      VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.render_fence));
      VK_CHECK(
          vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.swapchain_semaphore));
      VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.render_semaphore));

      // Initialize Descriptor Pool
    }
  }

  void FrameData::cleanup(const VkDevice device) {
	for (auto& frame : frames_) {
	  vkDestroyCommandPool(device, frame.command_pool, nullptr);
	  vkDestroyFence(device, frame.render_fence, nullptr);
	  vkDestroySemaphore(device, frame.swapchain_semaphore, nullptr);
	  vkDestroySemaphore(device, frame.render_semaphore, nullptr);
	}
  }

  FrameData::FrameResources& FrameData::get_current_frame() {
    return frames_[frame_->get_current_frame_index()];
  }

  VkDescriptorSet DescriptorAllocatorGrowable::allocate(
      VkDevice device, VkDescriptorSetLayout layout,
      const std::vector<uint32_t>& variableDescriptorCounts) {
    // get or create a pool to allocate from
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo = {};
    variableDescriptorCountInfo.sType
        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountInfo.descriptorSetCount = variableDescriptorCounts.size();
    variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data();

    VkDescriptorSetAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext
        = variableDescriptorCounts.empty()
              ? nullptr
              : &variableDescriptorCountInfo;  // Chain the variable descriptor count info
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    // allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
      fullPools.push_back(poolToUse);

      poolToUse = get_pool(device);
      allocInfo.descriptorPool = poolToUse;

      VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    } else {
      VK_CHECK(result);
    }

    readyPools.push_back(poolToUse);
    return ds;
  }

  void DescriptorAllocatorGrowable::init(const VkDevice device, uint32_t initial_sets,
                                         const std::span<PoolSizeRatio> poolRatios) {
    ratios.clear();

    for (auto r : poolRatios) {
      ratios.push_back(r);
    }

    const VkDescriptorPool newPool = create_pool(device, initial_sets, poolRatios);

    setsPerPool = initial_sets * 1.5;  // grow it next allocation

    readyPools.push_back(newPool);
  }

  void DescriptorAllocatorGrowable::clear_pools(VkDevice device) {
    for (auto p : readyPools) {
      vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : fullPools) {
      vkResetDescriptorPool(device, p, 0);
      readyPools.push_back(p);
    }
    fullPools.clear();
  }

  void DescriptorAllocatorGrowable::destroy_pools(VkDevice device) {
    for (const auto p : readyPools) {
      vkDestroyDescriptorPool(device, p, nullptr);
    }
    readyPools.clear();
    for (const auto p : fullPools) {
      vkDestroyDescriptorPool(device, p, nullptr);
    }
    fullPools.clear();
  }
}  // namespace gestalt::graphics