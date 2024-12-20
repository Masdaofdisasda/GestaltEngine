﻿#include "MaterialSystem.hpp"

#include "VulkanCheck.hpp"
#include "Interface/IDescriptorLayoutBuilder.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceManager.hpp"

#include <ranges>

namespace gestalt::application {

  constexpr uint32 kWhite = 0xFFFFFFFF;  // White color for color and occlusion
  constexpr uint32 kDefaultMetallicRoughness
      = 0xFF00FFFF;                           // Green color representing metallic-roughness
  constexpr uint32 kFlatNormal = 0xFFFF8080;  // Flat normal
  constexpr uint32 kBlack = 0xFF000000;       // Black color for emissive
  constexpr uint32 kMagenta = 0xFFFF00FF;     // Magenta color for error textures

  void MaterialSystem::prepare() {

    create_buffers();

    create_default_material();

    fill_uniform_buffer();
    fill_images_buffer();
  }

  void MaterialSystem::create_buffers() {
    const auto& mat_buffers = repository_->material_buffers;

    descriptor_layout_builder_->clear();
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT, false, getMaxTextures())
                  .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mat_buffers->descriptor_buffer
        = resource_manager_->create_descriptor_buffer(descriptor_layout, 5);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    mat_buffers->uniform_buffer = resource_manager_->create_buffer(
        sizeof(PbrMaterial::PbrConstants) * getMaxMaterials(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, "Material Data Buffer");
  }

  void MaterialSystem::create_default_material() {

    auto& default_mat = repository_->default_material_;

    default_mat.color_image = resource_manager_->create_image(
        (void*)&kWhite, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_mat.color_image);

    default_mat.metallic_roughness_image
        = resource_manager_->create_image((void*)&kDefaultMetallicRoughness, VkExtent3D{1, 1, 1},
                                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_mat.metallic_roughness_image);

    default_mat.normal_image
        = resource_manager_->create_image((void*)&kFlatNormal, VkExtent3D{1, 1, 1},
                                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_mat.normal_image);

    default_mat.emissive_image = resource_manager_->create_image(
        (void*)&kBlack, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_mat.emissive_image);

    default_mat.occlusion_image = resource_manager_->create_image(
        (void*)&kWhite, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_mat.occlusion_image);

    // checkerboard image for error textures and testing
    constexpr size_t checkerboard_size = 256;
    std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
        pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? kMagenta : kBlack;
      }
    }
    default_mat.error_checkerboard_image = resource_manager_->create_image(
        pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_mat.error_checkerboard_image);

    default_mat.color_sampler = repository_->get_sampler();
    default_mat.metallic_roughness_sampler
        = repository_->get_sampler({.magFilter = VK_FILTER_NEAREST,
                                    .minFilter = VK_FILTER_NEAREST, .anisotropyEnable = VK_FALSE});
    default_mat.normal_sampler = repository_->get_sampler();
    default_mat.emissive_sampler = repository_->get_sampler();
    default_mat.occlusion_sampler = repository_->get_sampler({.magFilter = VK_FILTER_NEAREST,
                                    .minFilter = VK_FILTER_NEAREST, .anisotropyEnable = VK_FALSE
  });

    PbrMaterial pbr_mat{};

    // default the material textures
    pbr_mat.textures.albedo_image = default_mat.color_image;
    pbr_mat.textures.albedo_sampler = default_mat.color_sampler;
    pbr_mat.textures.metal_rough_image = default_mat.metallic_roughness_image;
    pbr_mat.textures.metal_rough_sampler = default_mat.metallic_roughness_sampler;
    pbr_mat.textures.normal_image = default_mat.normal_image;
    pbr_mat.textures.normal_sampler = default_mat.normal_sampler;
    pbr_mat.textures.emissive_image = default_mat.emissive_image;
    pbr_mat.textures.emissive_sampler = default_mat.emissive_sampler;
    pbr_mat.textures.occlusion_image = default_mat.occlusion_image;
    pbr_mat.textures.occlusion_sampler = default_mat.occlusion_sampler;

    // build material
    const size_t material_id = repository_->materials.size();
    const std::string key = "default_material";
    repository_->materials.add(Material{.name = key, .config = pbr_mat});
    fmt::print("creating material {}, mat_id {}\n", key, material_id);
    
    resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");
  }

  void MaterialSystem::fill_uniform_buffer() {
    const auto& constant_buffer = repository_->material_buffers->uniform_buffer;

    const std::vector<PbrMaterial::PbrConstants> material_constants(getMaxMaterials());

    PbrMaterial::PbrConstants* uniformBufferPointer;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), constant_buffer->allocation,
                          (void**)&uniformBufferPointer));
    memcpy(uniformBufferPointer, material_constants.data(),
           sizeof(PbrMaterial::PbrConstants) * getMaxMaterials());

    vmaUnmapMemory(gpu_->getAllocator(), constant_buffer->allocation);

    repository_->material_buffers->descriptor_buffer
        ->write_buffer(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, constant_buffer->address,
                       sizeof(PbrMaterial::PbrConstants) * getMaxMaterials())
        .update();
  }

  void MaterialSystem::fill_images_buffer() {
    const auto& mat_buffers = repository_->material_buffers;
    const auto& default_mat = repository_->default_material_;

    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(getMaxTextures());

    for (int i = 0; i < getMaxMaterials(); ++i) {
      image_infos.push_back({default_mat.color_sampler, default_mat.color_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.metallic_roughness_sampler,
                             default_mat.metallic_roughness_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.normal_sampler, default_mat.normal_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.emissive_sampler, default_mat.emissive_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.occlusion_sampler, default_mat.occlusion_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }

    mat_buffers->descriptor_buffer
        ->write_image_array(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_infos)
        .update();
  }

  void MaterialSystem::update(float delta_time, const UserInput& movement, float aspect) {
    bool first_processed = false;

    for (size_t material_id = 0; material_id < repository_->materials.size(); ++material_id) {
      auto& [name, config, is_dirty] = repository_->materials.get(material_id);
      if (is_dirty) {
        if (!first_processed) {
          first_processed = true;
          vkDeviceWaitIdle(gpu_->getDevice());
        }

        write_material(config, material_id);
        is_dirty = false;
      }
    }

    if (first_processed) {
      // TODO batch update textures
    }

  }

  void MaterialSystem::cleanup() {
    const auto& material_buffers = repository_->material_buffers;
    resource_manager_->destroy_descriptor_buffer(material_buffers->descriptor_buffer);
    resource_manager_->destroy_image(material_buffers->bdrf_lut);
    resource_manager_->destroy_image(material_buffers->environment_irradiance_map);
    resource_manager_->destroy_image(material_buffers->environment_map);
    vkDestroySampler(gpu_->getDevice(), material_buffers->cube_map_sampler, nullptr);

    for (auto texture : repository_->textures.data()) {
      resource_manager_->destroy_image(texture);
    }
    repository_->textures.clear();

    repository_->materials.clear();

    resource_manager_->destroy_buffer(material_buffers->uniform_buffer);

    for (const auto& sampler : repository_->sampler_cache | std::views::values) {
      vkDestroySampler(gpu_->getDevice(), sampler, nullptr);
    }

    repository_->sampler_cache.clear();
  }

  void MaterialSystem::write_material(PbrMaterial& material, const uint32 material_id) {
    assert(material_id < getMaxMaterials());

    const auto& mat_buffers = repository_->material_buffers;

    // Create VkDescriptorImageInfo for each texture of the material
    std::vector<VkDescriptorImageInfo> image_infos
        = {{material.textures.albedo_sampler, material.textures.albedo_image.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
           {material.textures.metal_rough_sampler, material.textures.metal_rough_image.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
           {material.textures.normal_sampler, material.textures.normal_image.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
           {material.textures.emissive_sampler, material.textures.emissive_image.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
           {material.textures.occlusion_sampler, material.textures.occlusion_image.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

    // Calculate the start offset in the descriptor buffer
    const uint32_t texture_start = getPbrMaterialTextures() * material_id;

    mat_buffers->descriptor_buffer
        ->write_image_array(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_infos,
                            texture_start)
        .update();

    if (material.constants.albedo_tex_index != kUnusedTexture) {
      material.constants.albedo_tex_index = texture_start;
    }
    if (material.constants.metal_rough_tex_index != kUnusedTexture) {
      material.constants.metal_rough_tex_index = texture_start + 1;
    }
    if (material.constants.normal_tex_index != kUnusedTexture) {
      material.constants.normal_tex_index = texture_start + 2;
    }
    if (material.constants.emissive_tex_index != kUnusedTexture) {
      material.constants.emissive_tex_index = texture_start + 3;
    }
    if (material.constants.occlusion_tex_index != kUnusedTexture) {
      material.constants.occlusion_tex_index = texture_start + 4;
    }

    PbrMaterial::PbrConstants* mappedData;
    vmaMapMemory(gpu_->getAllocator(), repository_->material_buffers->uniform_buffer->allocation,
                 (void**)&mappedData);
    memcpy(mappedData + material_id, &material.constants, sizeof(PbrMaterial::PbrConstants));
    vmaUnmapMemory(gpu_->getAllocator(), repository_->material_buffers->uniform_buffer->allocation);
  }

}  // namespace gestalt::application