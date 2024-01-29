#pragma once
#include <fastgltf/types.hpp>

#include "vk_gpu.h"
#include "vk_types.h"
#include "vk_descriptors.h"
#include "materials.h"

class resource_manager {
  vk_gpu gpu_ = {};

public:
  // scene data for the gpu
  gpu_mesh_buffers scene_geometry_;
  AllocatedBuffer material_data_buffer_;

  DescriptorAllocatorGrowable descriptorPool;
  descriptor_writer writer;
  VkDescriptorSet materialSet;
  VkDescriptorSetLayout materialLayout;

  void init(const vk_gpu& gpu);

  void cleanup();

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);

  void upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  AllocatedImage create_cubemap(std::array<void*, 6> face_data, VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage, bool mipmapped = false);
  std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image);
  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false, bool cubemap = false);
  void write_material(const gltf_material::MaterialResources& resources, uint32_t material_id);
  void destroy_image(const AllocatedImage& img);
};
