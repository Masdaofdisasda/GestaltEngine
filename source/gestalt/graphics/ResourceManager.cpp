#include "ResourceManager.h"

#include "vk_images.h"
#include "vk_initializers.h"

#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "Components.h"
#include "CubemapUtils.h"


namespace gestalt {
  namespace graphics {
    using namespace application;
    using namespace foundation;

    AllocatedBuffer ResourceManager::create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                                   VmaMemoryUsage memoryUsage) {
      // allocate buffer
      VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
      bufferInfo.pNext = nullptr;
      bufferInfo.size = allocSize;

      bufferInfo.usage = usage;

      VmaAllocationCreateInfo vmaallocInfo = {};
      vmaallocInfo.usage = memoryUsage;
      vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
      AllocatedBuffer newBuffer;

      // allocate the buffer
      VK_CHECK(vmaCreateBuffer(gpu_.allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer,
                               &newBuffer.allocation, &newBuffer.info));
      return newBuffer;
    }

    void ResourceManager::init(const Gpu& gpu, const std::shared_ptr<Repository> repository) {
      gpu_ = gpu;
      repository_ = repository;

      per_frame_data_buffer = create_buffer(
          sizeof(PerFrameData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

      material_data.constants_buffer
          = create_buffer(sizeof(PbrMaterial::MaterialConstants) * kLimits.max_materials,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

      per_frame_data_layout
          = DescriptorLayoutBuilder()
                .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(gpu_.device, true);
      material_data.resource_layout = DescriptorLayoutBuilder()
                                          .add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                       VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                          .build(gpu_.device);
      material_data.constants_layout = DescriptorLayoutBuilder()
                                           .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                           .build(gpu_.device);
      ibl_data.IblLayout = DescriptorLayoutBuilder()
                               .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                               .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                               .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                               .build(gpu_.device);

      std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
          = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kLimits.max_textures},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kLimits.max_materials}};

      descriptorPool.init(gpu_.device, 1, sizes);

      material_data.resource_set = descriptorPool.allocate(
          gpu_.device, material_data.resource_layout, {kLimits.max_textures});
      material_data.constants_set = descriptorPool.allocate(
          gpu_.device, material_data.constants_layout, {kLimits.max_materials});
      ibl_data.IblSet = descriptorPool.allocate(gpu_.device, ibl_data.IblLayout);

      light_data.light_layout
          = DescriptorLayoutBuilder()
                .add_binding(15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                             false, kLimits.max_directional_lights)
                .add_binding(16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                             false, kLimits.max_point_lights)
                .add_binding(17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                             false, kLimits.max_directional_lights + kLimits.max_point_lights)
                .build(gpu_.device);

      scene_geometry_.vertex_layout
          = DescriptorLayoutBuilder()
                .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .build(gpu_.device);

      scene_geometry_.vertex_set
          = descriptorPool.allocate(gpu_.device, scene_geometry_.vertex_layout);

      {
        std::vector<GpuVertexPosition> vertex_positions(184521);

        GpuVertexPosition* mappedData;
        VK_CHECK(vmaMapMemory(gpu_.allocator, material_data.constants_buffer.allocation,
                              (void**)&mappedData));
        memcpy(mappedData, vertex_positions.data(),
               sizeof(PbrMaterial::MaterialConstants) * kLimits.max_materials);

        vmaUnmapMemory(gpu_.allocator, material_data.constants_buffer.allocation);

        std::vector<VkDescriptorBufferInfo> bufferInfos;
        for (int i = 0; i < vertex_positions.size(); ++i) {
          VkDescriptorBufferInfo bufferInfo = {};
          bufferInfo.buffer = material_data.constants_buffer.buffer;
          bufferInfo.offset = sizeof(PbrMaterial::MaterialConstants) * i;
          bufferInfo.range = sizeof(PbrMaterial::MaterialConstants);
          bufferInfos.push_back(bufferInfo);
        }
      }
    }

    void ResourceManager::init_default_data() {
      auto& default_material = repository_->default_material_;

      uint32_t white = 0xFFFFFFFF;  // White color for color and occlusion
      uint32_t default_metallic_roughness
          = 0xFF00FF00;                   // Green color representing metallic-roughness
      uint32_t flat_normal = 0xFFFF8080;  // Flat normal
      uint32_t black = 0xFF000000;        // Black color for emissive

      default_material.color_image = create_image(
          (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.color_image);

      default_material.metallic_roughness_image
          = create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                         VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.metallic_roughness_image);

      default_material.normal_image
          = create_image((void*)&flat_normal, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.normal_image);

      default_material.emissive_image = create_image(
          (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.emissive_image);

      default_material.occlusion_image = create_image(
          (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.occlusion_image);

      // checkerboard image for error textures and testing
      uint32_t magenta = 0xFFFF00FF;
      constexpr size_t checkerboard_size = 256;
      std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
      for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
          pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
      }
      default_material.error_checkerboard_image
          = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.error_checkerboard_image);

      VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

      sampler.magFilter = VK_FILTER_NEAREST;
      sampler.minFilter = VK_FILTER_NEAREST;

      vkCreateSampler(gpu_.device, &sampler, nullptr, &default_material.nearestSampler);
      repository_->samplers.add(default_material.nearestSampler);

      sampler.magFilter = VK_FILTER_LINEAR;
      sampler.minFilter = VK_FILTER_LINEAR;
      vkCreateSampler(gpu_.device, &sampler, nullptr, &default_material.linearSampler);
      repository_->samplers.add(default_material.linearSampler);

      PbrMaterial pbr_material{};
      pbr_material.constants.albedo_factor.x = 1.f;
      pbr_material.constants.albedo_factor.y = 1.f;
      pbr_material.constants.albedo_factor.z = 1.f;
      pbr_material.constants.albedo_factor.w = 1.f;

      pbr_material.constants.metal_rough_factor.x = 0.f;
      pbr_material.constants.metal_rough_factor.y = 0.f;
      // write material parameters to buffer

      // default the material textures
      pbr_material.resources.albedo_image = default_material.color_image;
      pbr_material.resources.albedo_sampler = default_material.linearSampler;
      pbr_material.resources.metal_rough_image = default_material.metallic_roughness_image;
      pbr_material.resources.metal_rough_sampler = default_material.linearSampler;
      pbr_material.resources.normal_image = default_material.normal_image;
      pbr_material.resources.normal_sampler = default_material.linearSampler;
      pbr_material.resources.emissive_image = default_material.emissive_image;
      pbr_material.resources.emissive_sampler = default_material.linearSampler;
      pbr_material.resources.occlusion_image = default_material.occlusion_image;
      pbr_material.resources.occlusion_sampler = default_material.nearestSampler;

      {
        // build material
        const size_t material_id = repository_->materials.size();
        const std::string key = "default_material";
        write_material(pbr_material, material_id);
        repository_->materials.add(Material{.name = key, .config = pbr_material});
        fmt::print("creating material {}, mat_id {}\n", key, material_id);
      }

      std::vector<PbrMaterial::MaterialConstants> material_constants(kLimits.max_materials);

      PbrMaterial::MaterialConstants* mappedData;
      VK_CHECK(vmaMapMemory(gpu_.allocator, material_data.constants_buffer.allocation,
                            (void**)&mappedData));
      memcpy(mappedData, material_constants.data(),
             sizeof(PbrMaterial::MaterialConstants) * kLimits.max_materials);

      vmaUnmapMemory(gpu_.allocator, material_data.constants_buffer.allocation);

      std::vector<VkDescriptorBufferInfo> bufferInfos;
      for (int i = 0; i < material_constants.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = material_data.constants_buffer.buffer;
        bufferInfo.offset = sizeof(PbrMaterial::MaterialConstants) * i;
        bufferInfo.range = sizeof(PbrMaterial::MaterialConstants);
        bufferInfos.push_back(bufferInfo);
      }

      writer.clear();
      writer.write_buffer_array(5, bufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
      writer.update_set(gpu_.device, material_data.constants_set);

      writer.clear();
      std::vector<VkDescriptorImageInfo> imageInfos{kLimits.max_textures};
      for (int i = 0; i < kLimits.max_textures; ++i) {
        VkDescriptorImageInfo image_info;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = repository_->default_material_.error_checkerboard_image.imageView;
        image_info.sampler = repository_->default_material_.linearSampler;
        imageInfos[i] = image_info;
      }
      writer.write_image_array(4, imageInfos, 0);
      writer.update_set(gpu_.device, material_data.resource_set);
    }

    void ResourceManager::cleanup() {
      for (auto& image : repository_->textures.data()) {
        destroy_image(image);
      }
      for (const auto& sampler : repository_->samplers.data()) {
        vkDestroySampler(gpu_.device, sampler, nullptr);
      }

      vkDestroyDescriptorSetLayout(gpu_.device, material_data.constants_layout, nullptr);
      vkDestroyDescriptorSetLayout(gpu_.device, ibl_data.IblLayout, nullptr);
      vkDestroyDescriptorSetLayout(gpu_.device, material_data.resource_layout, nullptr);
      destroy_buffer(material_data.constants_buffer);
      descriptorPool.destroy_pools(gpu_.device);
    }

    void ResourceManager::upload_mesh() {
      const std::span indices = repository_->indices.data();
      const std::span vertex_positions = repository_->vertex_positions.data();
      const std::span vertex_data = repository_->vertex_data.data();

      const size_t vertex_position_buffer_size
          = vertex_positions.size() * sizeof(GpuVertexPosition);
      const size_t vertex_data_buffer_size = vertex_data.size() * sizeof(GpuVertexData);
      const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

      // create vertex buffer
      scene_geometry_.vertexPositionBuffer
          = create_buffer(vertex_position_buffer_size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);
      scene_geometry_.vertexDataBuffer
          = create_buffer(vertex_data_buffer_size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);

      // create index buffer
      scene_geometry_.indexBuffer = create_buffer(
          index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);

      AllocatedBuffer staging
          = create_buffer(vertex_position_buffer_size + vertex_data_buffer_size + index_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

      void* data;
      VmaAllocation allocation = staging.allocation;
      VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &data));

      // copy vertex buffer
      memcpy(data, vertex_positions.data(), vertex_position_buffer_size);
      memcpy((char*)data + vertex_position_buffer_size, vertex_data.data(),
             vertex_data_buffer_size);
      // copy index buffer
      memcpy((char*)data + vertex_position_buffer_size + vertex_data_buffer_size, indices.data(),
             index_buffer_size);

      gpu_.immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertex_positions_copy;
        vertex_positions_copy.dstOffset = 0;
        vertex_positions_copy.srcOffset = 0;
        vertex_positions_copy.size = vertex_position_buffer_size;

        vkCmdCopyBuffer(cmd, staging.buffer, scene_geometry_.vertexPositionBuffer.buffer, 1,
                        &vertex_positions_copy);

        VkBufferCopy vertex_data_copy;
        vertex_data_copy.dstOffset = 0;
        vertex_data_copy.srcOffset = vertex_position_buffer_size;
        vertex_data_copy.size = vertex_data_buffer_size;

        vkCmdCopyBuffer(cmd, staging.buffer, scene_geometry_.vertexDataBuffer.buffer, 1,
                        &vertex_data_copy);

        VkBufferCopy index_copy;
        index_copy.dstOffset = 0;
        index_copy.srcOffset = vertex_position_buffer_size + vertex_data_buffer_size;
        index_copy.size = index_buffer_size;

        vkCmdCopyBuffer(cmd, staging.buffer, scene_geometry_.indexBuffer.buffer, 1, &index_copy);
      });

      vmaUnmapMemory(gpu_.allocator, allocation);
      destroy_buffer(staging);

      writer.clear();
      writer.write_buffer(0, scene_geometry_.vertexPositionBuffer.buffer,
                          vertex_position_buffer_size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer.write_buffer(1, scene_geometry_.vertexDataBuffer.buffer, vertex_data_buffer_size, 0,
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer.update_set(gpu_.device, scene_geometry_.vertex_set);
    }

    void ResourceManager::destroy_buffer(const AllocatedBuffer& buffer) {
      vmaDestroyBuffer(gpu_.allocator, buffer.buffer, buffer.allocation);
    }

    VkSampler ResourceManager::create_sampler(const VkSamplerCreateInfo& sampler_create_info) const {
      VkSampler new_sampler;
      vkCreateSampler(gpu_.device, &sampler_create_info, nullptr, &new_sampler);
      return new_sampler;
    }

    AllocatedImage ResourceManager::create_image(VkExtent3D size, VkFormat format,
                                                 VkImageUsageFlags usage, bool mipmapped,
                                                 bool cubemap) {
      AllocatedImage newImage;
      newImage.imageFormat = format;
      newImage.imageExtent = size;

      VkImageCreateInfo img_info;
      if (cubemap) {
        img_info = vkinit::cubemap_create_info(format, usage, size);
      } else {
        img_info = vkinit::image_create_info(format, usage, size);
      }

      if (mipmapped) {
        img_info.mipLevels
            = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
      }

      // always allocate images on dedicated GPU memory
      VmaAllocationCreateInfo allocinfo = {};
      allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      // allocate and create the image
      VK_CHECK(vmaCreateImage(gpu_.allocator, &img_info, &allocinfo, &newImage.image,
                              &newImage.allocation, nullptr));

      // if the format is a depth format, we will need to have it use the correct
      // aspect flag
      VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
      if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
      }

      if (cubemap) {
        // Build an image-view for the image
        VkImageViewCreateInfo view_info
            = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;  // View type for cubemap
        view_info.subresourceRange.levelCount = img_info.mipLevels;
        view_info.subresourceRange.layerCount = 6;  // Layer count for cubemap

        VK_CHECK(vkCreateImageView(gpu_.device, &view_info, nullptr, &newImage.imageView));

        return newImage;
      }

      // build a image-view for the image
      VkImageViewCreateInfo view_info
          = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
      view_info.subresourceRange.levelCount = img_info.mipLevels;

      VK_CHECK(vkCreateImageView(gpu_.device, &view_info, nullptr, &newImage.imageView));

      return newImage;
    }

    AllocatedImage ResourceManager::create_image(void* data, VkExtent3D size, VkFormat format,
                                                 VkImageUsageFlags usage, bool mipmapped) {
      size_t data_size = static_cast<size_t>(size.depth * size.width * size.height) * 4;
      AllocatedBuffer uploadbuffer
          = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

      memcpy(uploadbuffer.info.pMappedData, data, data_size);

      AllocatedImage new_image = create_image(
          size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          mipmapped);

      gpu_.immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
          vkutil::generate_mipmaps(
              cmd, new_image.image,
              VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
        } else {
          vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
      });
      destroy_buffer(uploadbuffer);
      return new_image;
    }

    AllocatedImage ResourceManager::create_cubemap(const void* imageData, VkExtent3D size,
                                                   VkFormat format, VkImageUsageFlags usage,
                                                   bool mipmapped) {
      size_t faceWidth = size.width;
      size_t faceHeight = size.height;
      size_t numChannels = 4;  // Assuming RGBA
      size_t bytesPerFloat = sizeof(float);

      size_t faceSizeBytes = faceWidth * faceHeight * numChannels * bytesPerFloat;
      size_t totalCubemapSizeBytes = faceSizeBytes * 6;

      // Create a buffer large enough to hold all faces
      AllocatedBuffer uploadbuffer = create_buffer(
          totalCubemapSizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

      // Copy each face data into the buffer
      memcpy(uploadbuffer.info.pMappedData, imageData, totalCubemapSizeBytes);

      // Create the image with VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag for cubemaps
      AllocatedImage new_image = create_image(
          size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          mipmapped, true);

      gpu_.immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Copy each face from the buffer to the image
        for (int i = 0; i < 6; ++i) {
          VkBufferImageCopy copyRegion = {};
          copyRegion.bufferOffset = faceSizeBytes * i;
          copyRegion.bufferRowLength = 0;
          copyRegion.bufferImageHeight = 0;

          copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          copyRegion.imageSubresource.mipLevel = 0;
          copyRegion.imageSubresource.baseArrayLayer = i;  // Specify the face index
          copyRegion.imageSubresource.layerCount = 1;
          copyRegion.imageExtent = size;

          vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
        }

        if (mipmapped) {
          // vkutil::generate_mipmaps( TODO
          // cmd, new_image.image,
          // VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
        } else {
          vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL);
        }
      });
      destroy_buffer(uploadbuffer);
      return new_image;
    }

    void ResourceManager::write_material(PbrMaterial& material, const uint32_t material_id) {
      writer.clear();

      std::vector<VkDescriptorImageInfo> imageInfos = {
          {material.resources.albedo_sampler, material.resources.albedo_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.resources.metal_rough_sampler, material.resources.metal_rough_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.resources.normal_sampler, material.resources.normal_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.resources.emissive_sampler, material.resources.emissive_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.resources.occlusion_sampler, material.resources.occlusion_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

      const uint32_t texture_start = imageInfos.size() * material_id;
      writer.write_image_array(4, imageInfos, texture_start);

      writer.update_set(gpu_.device, material_data.resource_set);

      if (material.constants.albedo_tex_index != unused_texture) {
        material.constants.albedo_tex_index = texture_start;
      }
      if (material.constants.metal_rough_tex_index != unused_texture) {
        material.constants.metal_rough_tex_index = texture_start + 1;
      }
      if (material.constants.normal_tex_index != unused_texture) {
        material.constants.normal_tex_index = texture_start + 2;
      }
      if (material.constants.emissive_tex_index != unused_texture) {
        material.constants.emissive_tex_index = texture_start + 3;
      }
      if (material.constants.occlusion_tex_index != unused_texture) {
        material.constants.occlusion_tex_index = texture_start + 4;
      }

      PbrMaterial::MaterialConstants* mappedData;
      vmaMapMemory(gpu_.allocator, material_data.constants_buffer.allocation, (void**)&mappedData);
      memcpy(mappedData + material_id, &material.constants, sizeof(PbrMaterial::MaterialConstants));
      vmaUnmapMemory(gpu_.allocator, material_data.constants_buffer.allocation);

      VkDescriptorBufferInfo buffer_info;
      buffer_info.buffer = material_data.constants_buffer.buffer;
      buffer_info.offset = sizeof(PbrMaterial::MaterialConstants) * material_id;
      buffer_info.range = sizeof(PbrMaterial::MaterialConstants);
      std::vector bufferInfos = {buffer_info};

      writer.clear();
      writer.write_buffer_array(5, bufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
      writer.update_set(gpu_.device, material_data.constants_set);
    }

    std::optional<AllocatedImage> ResourceManager::load_image(const std::string& filepath) {
      AllocatedImage new_image;

      int width, height, nrChannels;
      unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 4);
      if (data) {
        VkExtent3D imageSize;
        imageSize.width = width;
        imageSize.height = height;
        imageSize.depth = 1;

        new_image = create_image(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_SAMPLED_BIT, true);

        stbi_image_free(data);
      } else {
        return {};
      }

      if (new_image.image == VK_NULL_HANDLE) {
        return {};
      }
      return new_image;
    }

    static void float24to32(int w, int h, const float* img24, float* img32) {
      const int numPixels = w * h;
      for (int i = 0; i != numPixels; i++) {
        *img32++ = *img24++;
        *img32++ = *img24++;
        *img32++ = *img24++;
        *img32++ = 1.0f;
      }
    }

    void ResourceManager::load_and_create_cubemap(const std::string& file_path,
                                                  AllocatedImage& cubemap) {
      int w, h, comp;
      float* img = stbi_loadf(file_path.c_str(), &w, &h, &comp, 3);
      if (!img) {
        fmt::print("Failed to load image: {}\n", file_path);
        fflush(stdout);
      }

      std::vector<float> img32(w * h * 4);
      float24to32(w, h, img, img32.data());  // Convert HDR format as needed

      cubemap = create_cubemap_from_HDR(img32, h,
                                        w);  // Assume this function initializes cubemap correctly
      stbi_image_free(img);
    }

    void ResourceManager::load_and_process_cubemap(const std::string& file_path) {
      size_t lastDotIndex = file_path.find_last_of('.');
      if (lastDotIndex == std::string::npos) {
        fmt::print("Failed to find file extension in file path: {}\n", file_path);
        return;
      }
      std::string base_path = file_path.substr(0, lastDotIndex);
      std::string extension = file_path.substr(lastDotIndex);  // Includes the dot

      std::string env_file = base_path + "_environment" + extension;
      std::string irr_file = base_path + "_irradiance" + extension;

      if (std::filesystem::exists(env_file) && std::filesystem::exists(irr_file)) {
        fmt::print("Loading environment map from file: {}\n", env_file);
        load_and_create_cubemap(env_file, ibl_data.environment_map);

        fmt::print("Loading irradiance map from file: {}\n", irr_file);
        load_and_create_cubemap(irr_file, ibl_data.environment_irradiance_map);
      } else {
        fmt::print("Loading HDR image from file: {}\n", file_path);
        int w, h, comp;
        float* original_img = stbi_loadf(file_path.c_str(), &w, &h, &comp, 3);
        if (!original_img) {
          fmt::print("Failed to load image: {}\n", file_path);
          fflush(stdout);
          return;
        }

        // Process the HDR image for the environment map
        const int envDstW = 1024;
        const int envDstH = 512;
        std::vector<float> env_out(envDstW * envDstH * 3);  // Using float directly for simplicity
        downsample_equirectangular_map((glm::vec3*)original_img, w, h, envDstW, envDstH,
                                       (glm::vec3*)env_out.data());
        stbi_write_hdr(env_file.c_str(), envDstW, envDstH, 3, env_out.data());

        std::vector<float> img32_env(envDstW * envDstH * 4);
        float24to32(envDstW, envDstH, env_out.data(), img32_env.data());
        ibl_data.environment_map = create_cubemap_from_HDR(img32_env, envDstH, envDstW);

        // Process the HDR image for the irradiance map
        const int irrDstW = 256;
        const int irrDstH = 128;
        std::vector<float> irr_out(irrDstW * irrDstH * 3);  // Adjusted vector type
        convolveDiffuse((glm::vec3*)original_img, w, h, irrDstW, irrDstH,
                        (glm::vec3*)irr_out.data(), 1024);
        stbi_write_hdr(irr_file.c_str(), irrDstW, irrDstH, 3, irr_out.data());

        std::vector<float> img32_irr(irrDstW * irrDstH * 4);
        float24to32(irrDstW, irrDstH, irr_out.data(), img32_irr.data());
        ibl_data.environment_irradiance_map = create_cubemap_from_HDR(img32_irr, irrDstH, irrDstW);

        // Cleanup
        stbi_image_free(original_img);
      }

      // Update the sampler and descriptor set for both maps

      VkSamplerCreateInfo sampl
          = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
      sampl.maxLod = VK_LOD_CLAMP_NONE;
      sampl.minLod = 0;
      sampl.magFilter = VK_FILTER_LINEAR;
      sampl.minFilter = VK_FILTER_LINEAR;
      sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      vkCreateSampler(gpu_.device, &sampl, nullptr, &ibl_data.cube_map_sampler);

      ibl_data.bdrfLUT = load_image("../../assets/bdrf_lut.png").value();

      writer.clear();
      writer.write_image(1, ibl_data.environment_map.imageView, ibl_data.cube_map_sampler,
                         VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(2, ibl_data.environment_irradiance_map.imageView,
                         ibl_data.cube_map_sampler, VK_IMAGE_LAYOUT_GENERAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(
          3, ibl_data.bdrfLUT.imageView, repository_->default_material_.linearSampler,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, ibl_data.IblSet);
    }

    AllocatedImage ResourceManager::create_cubemap_from_HDR(std::vector<float>& image_data, int h,
                                                            int w) {
      Bitmap in(w, h, 4, eBitmapFormat_Float, image_data.data());
      Bitmap out_bitmap = convertEquirectangularMapToVerticalCross(in);

      Bitmap cube = convertVerticalCrossToCubeMapFaces(out_bitmap);

      return create_cubemap(cube.data_.data(),
                            {static_cast<uint32_t>(cube.w_), static_cast<uint32_t>(cube.h_), 1},
                            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);
    }

    void ResourceManager::create_color_frame_buffer(const VkExtent3D& extent,
                                                    AllocatedImage& color_image) const {
      assert(color_image.type == ImageType::kColor);
      // hardcoding the draw format to 32 bit float
      color_image.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
      color_image.imageExtent = extent;

      VkImageUsageFlags drawImageUsages{};
      drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
      drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

      VkImageCreateInfo rimg_info
          = vkinit::image_create_info(color_image.imageFormat, drawImageUsages, extent);

      // for the draw image, we want to allocate it from gpu local memory
      VmaAllocationCreateInfo rimg_allocinfo = {};
      rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      // allocate and create the image
      vmaCreateImage(gpu_.allocator, &rimg_info, &rimg_allocinfo, &color_image.image,
                     &color_image.allocation, nullptr);

      // build a image-view for the draw image to use for rendering
      VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
          color_image.imageFormat, color_image.image, VK_IMAGE_ASPECT_COLOR_BIT);

      VK_CHECK(vkCreateImageView(gpu_.device, &rview_info, nullptr, &color_image.imageView));
    }

    void ResourceManager::create_depth_frame_buffer(const VkExtent3D& extent,
                                                    AllocatedImage& depth_image) const {
      assert(depth_image.type == ImageType::kDepth);

      depth_image.imageFormat = VK_FORMAT_D32_SFLOAT;
      depth_image.imageExtent = extent;
      VkImageUsageFlags depthImageUsages{};
      depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

      VmaAllocationCreateInfo rimg_allocinfo = {};
      rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VkImageCreateInfo dimg_info
          = vkinit::image_create_info(depth_image.imageFormat, depthImageUsages, extent);

      // allocate and create the image
      vmaCreateImage(gpu_.allocator, &dimg_info, &rimg_allocinfo, &depth_image.image,
                     &depth_image.allocation, nullptr);

      // build a image-view for the draw image to use for rendering
      VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
          depth_image.imageFormat, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);

      VK_CHECK(vkCreateImageView(gpu_.device, &dview_info, nullptr, &depth_image.imageView));
    }

    void ResourceManager::create_framebuffer(const VkExtent3D& extent, FrameBuffer& frame_buffer) {
      AllocatedImage color_image{ImageType::kColor};
      AllocatedImage depth_image{ImageType::kDepth};

      create_color_frame_buffer(extent, color_image);
      create_depth_frame_buffer(extent, depth_image);

      frame_buffer.add_color_image(color_image);
      frame_buffer.add_depth_image(depth_image);
    }

    void ResourceManager::create_framebuffer(const VkExtent3D& extent,
                                             DoubleBufferedFrameBuffer& frame_buffer) {
      for (int i = 0; i < 2; ++i) {
        create_framebuffer(extent, frame_buffer.get_write_buffer());
        frame_buffer.switch_buffers();
      }
    }

    void ResourceManager::destroy_image(const AllocatedImage& img) {
      vkDestroyImageView(gpu_.device, img.imageView, nullptr);
      vmaDestroyImage(gpu_.allocator, img.image, img.allocation);
    }
  }  // namespace graphics
}  // namespace gestalt