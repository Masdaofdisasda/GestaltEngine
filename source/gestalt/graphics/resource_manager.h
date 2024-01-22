#pragma once
#include <fastgltf/types.hpp>

#include "vk_gpu.h"
#include "vk_types.h"

class resource_manager {
  vk_gpu gpu_ = {};

public:
  void init(const vk_gpu& gpu) {
    gpu_ = gpu;
  }

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);

  gpu_mesh_buffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image);
  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  void destroy_image(const AllocatedImage& img);
};
