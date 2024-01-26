#include <vk_descriptors.h>

DescriptorLayoutBuilder& DescriptorLayoutBuilder::add_binding(uint32_t binding,
                                                           VkDescriptorType type) {
  VkDescriptorSetLayoutBinding newbind{};
  newbind.binding = binding;
  newbind.descriptorCount = 1;
  newbind.descriptorType = type;

  bindings.push_back(newbind);

  return *this;
}

void DescriptorLayoutBuilder::clear() { bindings.clear(); }

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device,
                                                     VkShaderStageFlags shaderStages) {
  for (auto& b : bindings) {
    b.stageFlags |= shaderStages;
  }

  VkDescriptorSetLayoutCreateInfo info
      = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  info.pNext = nullptr;

  info.pBindings = bindings.data();
  info.bindingCount = static_cast<uint32_t>(bindings.size());
  info.flags = 0;

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

void DescriptorWriter::clear() {
  imageInfos.clear();
  writes.clear();
  bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set) {
  for (VkWriteDescriptorSet& write : writes) {
    write.dstSet = set;
  }

  vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
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
                                    std::span<PoolSizeRatio> poolRatios) {
  std::vector<VkDescriptorPoolSize> poolSizes;
  for (PoolSizeRatio ratio : poolRatios) {
    poolSizes.push_back(VkDescriptorPoolSize{.type = ratio.type,
                                             .descriptorCount = uint32_t(ratio.ratio * maxSets)});
  }

  VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool_info.flags = 0;
  pool_info.maxSets = maxSets;
  pool_info.poolSizeCount = (uint32_t)poolSizes.size();
  pool_info.pPoolSizes = poolSizes.data();

  vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device) {
  vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device) {
  vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
  VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
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
  if (readyPools.size() != 0) {
    newPool = readyPools.back();
    readyPools.pop_back();
  } else {
    // need to create a new pool
    newPool = create_pool(device, setsPerPool, ratios);

    setsPerPool = setsPerPool * 1.5;
    if (setsPerPool > 4092) {
      setsPerPool = 4092;
    }
  }

  return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount,
                                                          std::span<PoolSizeRatio> poolRatios) {
  std::vector<VkDescriptorPoolSize> poolSizes;
  for (PoolSizeRatio ratio : poolRatios) {
    poolSizes.push_back(VkDescriptorPoolSize{.type = ratio.type,
                                             .descriptorCount = uint32_t(ratio.ratio * setCount)});
  }

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = 0;
  pool_info.maxSets = setCount;
  pool_info.poolSizeCount = (uint32_t)poolSizes.size();
  pool_info.pPoolSizes = poolSizes.data();

  VkDescriptorPool newPool;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
  return newPool;
}

void vk_descriptor_manager::init(const vk_gpu& gpu, std::vector<frame_data>& frames) {
  gpu_ = gpu;
  deletion_service_.init(gpu_.device, gpu_.allocator);

  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
  };

  {
    DescriptorLayoutBuilder builder;
    gpu_scene_data_descriptor_layout
        = builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
              .build(gpu_.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    deletion_service_.push(gpu_scene_data_descriptor_layout);
  }

  for (auto& frame : frames) {
    // create a descriptor pool
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };

    frame.frame_descriptors = DescriptorAllocatorGrowable{};
    frame.frame_descriptors.init(gpu_.device, 1000, frame_sizes);

    auto& frame_descriptor = frame.frame_descriptors;
    deletion_service_.push_function(
        [this, &frame_descriptor]() { frame_descriptor.destroy_pools(gpu_.device); });
  }
}

void vk_descriptor_manager::cleanup() {
  deletion_service_.flush();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device,
                                                      VkDescriptorSetLayout layout) {
  // get or create a pool to allocate from
  VkDescriptorPool poolToUse = get_pool(device);

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.pNext = nullptr;
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
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
  }

  readyPools.push_back(poolToUse);
  return ds;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets,
                                       std::span<PoolSizeRatio> poolRatios) {
  ratios.clear();

  for (auto r : poolRatios) {
    ratios.push_back(r);
  }

  VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);

  setsPerPool = maxSets * 1.5;  // grow it next allocation

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
  for (auto p : readyPools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  readyPools.clear();
  for (auto p : fullPools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  fullPools.clear();
}