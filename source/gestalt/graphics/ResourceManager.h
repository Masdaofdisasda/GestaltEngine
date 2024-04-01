#pragma once
#include <fastgltf/types.hpp>

#include "Repository.h"
#include "Gpu.h"
#include "vk_types.h"
#include "vk_descriptors.h"

namespace gestalt {
  namespace graphics {

    class ResourceManager {
      Gpu gpu_ = {};
      std::shared_ptr<foundation::Repository> repository_;

      void load_and_create_cubemap(const std::string& file_path, foundation::AllocatedImage& cubemap);

    public:
      foundation::GpuMeshBuffers scene_geometry_;

      foundation::AllocatedBuffer per_frame_data_buffer;
      VkDescriptorSetLayout per_frame_data_layout;

      struct material_data {
        VkDescriptorSet resource_set;
        VkDescriptorSetLayout resource_layout;

        foundation::AllocatedBuffer constants_buffer;
        VkDescriptorSet constants_set;
        VkDescriptorSetLayout constants_layout;
      } material_data;

      struct ibl_data {
        foundation::AllocatedImage environment_map;
        foundation::AllocatedImage environment_irradiance_map;
        foundation::AllocatedImage bdrfLUT;

        VkSampler cube_map_sampler;

        VkDescriptorSet IblSet;
        VkDescriptorSetLayout IblLayout;
      } ibl_data;

      struct light_data {
        foundation::AllocatedBuffer dir_light_buffer;
        foundation::AllocatedBuffer point_light_buffer;
        foundation::AllocatedBuffer view_proj_matrices;

        VkDescriptorSet light_set = nullptr;
        VkDescriptorSetLayout light_layout;
      } light_data;

      DescriptorAllocatorGrowable* descriptor_pool;

      DescriptorAllocatorGrowable descriptorPool;  // TODO hide this
      descriptor_writer writer;

      foundation::DrawContext main_draw_context_;  // TODO cleanup
      foundation::PerFrameData per_frame_data_;
      foundation::RenderConfig config_;

      void init(const Gpu& gpu, const std::shared_ptr<foundation::Repository> repository);

      void cleanup();

      foundation::AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                                VmaMemoryUsage memoryUsage);

      void upload_mesh();
      void destroy_buffer(const foundation::AllocatedBuffer& buffer);

      VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const;

      foundation::AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format,
                                              VkImageUsageFlags usage, bool mipmapped = false);
      foundation::AllocatedImage create_cubemap(const void* imageData, VkExtent3D size, VkFormat format,
                                                VkImageUsageFlags usage, bool mipmapped = false);
      std::optional<foundation::AllocatedImage> load_image(const std::string& filepath);
      void init_default_data();
      void load_and_process_cubemap(const std::string& file_path);
      foundation::AllocatedImage create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w);
      void create_color_frame_buffer(const VkExtent3D& extent, foundation::AllocatedImage& color_image) const;
      void create_depth_frame_buffer(const VkExtent3D& extent, foundation::AllocatedImage& depth_image) const;
      void create_framebuffer(const VkExtent3D& extent, foundation::FrameBuffer& frame_buffer);
      void create_framebuffer(const VkExtent3D& extent, foundation::DoubleBufferedFrameBuffer& frame_buffer);
      foundation::AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                              bool mipmapped = false, bool cubemap = false);
      void write_material(foundation::PbrMaterial& material, const uint32_t material_id);
      void destroy_image(const foundation::AllocatedImage& img);
    };

  }  // namespace graphics
}  // namespace gestalt