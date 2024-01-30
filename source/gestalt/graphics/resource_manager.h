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
  AllocatedImage filtered_map;

  DescriptorAllocatorGrowable descriptorPool;
  descriptor_writer writer;
  VkDescriptorSet materialSet;
  VkDescriptorSet materialConstantsSet;
  VkDescriptorSetLayout materialLayout;
  VkDescriptorSetLayout materialConstantsLayout;

  void init(const vk_gpu& gpu);

  void cleanup();

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);

  void upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  AllocatedImage create_cubemap(const void* imageData, VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage, bool mipmapped = false);
  std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image);
  void load_and_process_cubemap(const std::string& file_path);
  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false, bool cubemap = false);
  void write_material(const gltf_material& material, uint32_t material_id);
  void destroy_image(const AllocatedImage& img);
};
