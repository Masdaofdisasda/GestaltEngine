#pragma once
#include <fastgltf/types.hpp>

#include "database.h"
#include "vk_gpu.h"
#include "vk_types.h"
#include "vk_descriptors.h"

class resource_manager {
  vk_gpu gpu_ = {};
  std::unique_ptr<database> database_ = std::make_unique<database>();
  std::unordered_map<std::string, std::shared_ptr<AllocatedImage>> resources_;


  void load_and_create_cubemap(const std::string& file_path, AllocatedImage& cubemap);
public:
  gpu_mesh_buffers scene_geometry_;

  AllocatedBuffer per_frame_data_buffer;
  VkDescriptorSetLayout per_frame_data_layout;

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
  AllocatedImage bdrfLUT;

  VkSampler cube_map_sampler;

  VkDescriptorSet IblSet;
  VkDescriptorSetLayout IblLayout;
  } ibl_data;

  struct light_data {
    AllocatedBuffer dir_light_buffer;
    AllocatedBuffer point_light_buffer;
    AllocatedBuffer view_proj_matrices;
  } light_data;

  template <typename T> void add_resource(const std::string& id, std::shared_ptr<T> resource) {
  resources_[id] = resource;
  }

  std::unordered_map<std::string, std::string> direct_original_mapping;
  template <typename T> std::shared_ptr<T> get_resource(const std::string& id) {
  if (const auto it = resources_.find(id); it != resources_.end()) {
    return std::dynamic_pointer_cast<T>(it->second);
  }
  const std::string original_id = direct_original_mapping[id];
  if (const auto it = resources_.find(original_id); it != resources_.end()) {
    return std::dynamic_pointer_cast<T>(it->second);
  }
  return nullptr;
  }

  bool update_resource_id(const std::string& oldId, const std::string& newId) {
  auto it = resources_.find(oldId);
  if (it == resources_.end()) {
    return false;
  }
  resources_[newId] = it->second;
  resources_.erase(it);
  return true;
  }

  DescriptorAllocatorGrowable* descriptor_pool;

  DescriptorAllocatorGrowable descriptorPool; //TODO hide this
  descriptor_writer writer;

  database& get_database() const { return *database_; }

  draw_context main_draw_context_; //TODO cleanup
  per_frame_data per_frame_data_;
  render_config config_;

  void init(const vk_gpu& gpu);

  void cleanup();
  void update_mesh(const std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices);

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);

  void upload_mesh();
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  AllocatedImage create_cubemap(const void* imageData, VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage, bool mipmapped = false);
  std::optional<AllocatedImage> load_image(const std::string& filepath);
  void init_default_data();
  void load_and_process_cubemap(const std::string& file_path);
  AllocatedImage create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w);
  void create_color_frame_buffer(const VkExtent3D& extent, AllocatedImage& color_image) const;
  void create_depth_frame_buffer(const VkExtent3D& extent, AllocatedImage& depth_image) const;
  void create_framebuffer(const VkExtent3D& extent, frame_buffer& frame_buffer);
  void create_framebuffer(const VkExtent3D& extent, double_buffered_frame_buffer& frame_buffer);
  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false, bool cubemap = false);
  void write_material(pbr_material& material, const uint32_t material_id);
  void destroy_image(const AllocatedImage& img);
};
