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
  AllocatedImage environment_map;
  AllocatedImage environment_irradiance_map;
  AllocatedImage bdrfLUT;

  DescriptorAllocatorGrowable descriptorPool;
  descriptor_writer writer;
  VkSampler cube_map_sampler;
  VkDescriptorSet materialSet;
  VkDescriptorSet materialConstantsSet;
  VkDescriptorSet IblSet;
  VkDescriptorSetLayout materialLayout;
  VkDescriptorSetLayout materialConstantsLayout;
  VkDescriptorSetLayout IblLayout;

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
  AllocatedImage create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w);
  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false, bool cubemap = false);
  void write_material(const gltf_material& material, uint32_t material_id);
  void destroy_image(const AllocatedImage& img);
};
