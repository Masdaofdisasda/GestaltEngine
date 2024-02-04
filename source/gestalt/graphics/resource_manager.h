#pragma once
#include <fastgltf/types.hpp>

#include "database.h"
#include "vk_gpu.h"
#include "vk_types.h"
#include "vk_descriptors.h"

class resource_manager {
  vk_gpu gpu_ = {};
  std::unique_ptr<database> database_ = std::make_unique<database>();

public:
  gpu_mesh_buffers scene_geometry_;
  AllocatedBuffer per_frame_data_buffer;

  struct material_data {
  VkDescriptorSet resource_set;
  VkDescriptorSetLayout resource_layout;

  AllocatedBuffer constants_buffer;
  VkDescriptorSet constants_set;
  VkDescriptorSetLayout constants_layout;
  } material_data;

  struct ibl_data {
  AllocatedImage environment_map;
  AllocatedImage environment_irradiance_map;
  AllocatedImage bdrfLUT; // TODO generate this

  VkSampler cube_map_sampler;

  VkDescriptorSet IblSet;
  VkDescriptorSetLayout IblLayout;
  } ibl_data;

  DescriptorAllocatorGrowable descriptorPool;
  descriptor_writer writer;

  database& get_database() const { return *database_; }

  void init(const vk_gpu& gpu);

  void cleanup();

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);

  void upload_mesh();
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
  void write_material(const pbr_material& material, const uint32_t material_id);
  void destroy_image(const AllocatedImage& img);
};
