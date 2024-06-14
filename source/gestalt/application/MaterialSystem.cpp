#include "SceneSystem.hpp"

namespace gestalt::application {

  void MaterialSystem::write_material(PbrMaterial& material, const uint32 material_id) {
    writer_->clear();

    std::vector<VkDescriptorImageInfo> imageInfos
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

    const uint32 texture_start = imageInfos.size() * material_id;
    writer_->write_image_array(4, imageInfos, texture_start);

    writer_->update_set(gpu_->getDevice(), repository_->material_data.resource_set);

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
    vmaMapMemory(gpu_->getAllocator(), repository_->material_data.constants_buffer.allocation,
                 (void**)&mappedData);
    memcpy(mappedData + material_id, &material.constants, sizeof(PbrMaterial::PbrConstants));
    vmaUnmapMemory(gpu_->getAllocator(), repository_->material_data.constants_buffer.allocation);
  }

  void MaterialSystem::create_defaults() {
    auto& default_material = repository_->default_material_;

    uint32_t white = 0xFFFFFFFF;  // White color for color and occlusion
    uint32_t default_metallic_roughness
        = 0xFF00FFFF;                   // Green color representing metallic-roughness
    uint32_t flat_normal = 0xFFFF8080;  // Flat normal
    uint32_t black = 0xFF000000;        // Black color for emissive

    default_material.color_image = resource_manager_->create_image(
        (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_material.color_image);

    default_material.metallic_roughness_image
        = resource_manager_->create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_material.metallic_roughness_image);

    default_material.normal_image
        = resource_manager_->create_image((void*)&flat_normal, VkExtent3D{1, 1, 1},
                                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_material.normal_image);

    default_material.emissive_image = resource_manager_->create_image(
        (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_material.emissive_image);

    default_material.occlusion_image = resource_manager_->create_image(
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
    default_material.error_checkerboard_image = resource_manager_->create_image(
        pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    repository_->textures.add(default_material.error_checkerboard_image);

    VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(gpu_->getDevice(), &sampler, nullptr, &default_material.nearestSampler);
    repository_->samplers.add(default_material.nearestSampler);

    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(gpu_->getDevice(), &sampler, nullptr, &default_material.linearSampler);
    repository_->samplers.add(default_material.linearSampler);

    PbrMaterial pbr_material{};

    // default the material textures
    pbr_material.textures.albedo_image = default_material.color_image;
    pbr_material.textures.albedo_sampler = default_material.linearSampler;
    pbr_material.textures.metal_rough_image = default_material.metallic_roughness_image;
    pbr_material.textures.metal_rough_sampler = default_material.linearSampler;
    pbr_material.textures.normal_image = default_material.normal_image;
    pbr_material.textures.normal_sampler = default_material.linearSampler;
    pbr_material.textures.emissive_image = default_material.emissive_image;
    pbr_material.textures.emissive_sampler = default_material.linearSampler;
    pbr_material.textures.occlusion_image = default_material.occlusion_image;
    pbr_material.textures.occlusion_sampler = default_material.nearestSampler;

    {
      // build material
      const size_t material_id = repository_->materials.size();
      const std::string key = "default_material";
      write_material(pbr_material, material_id);
      repository_->materials.add(Material{.name = key, .config = pbr_material});
      fmt::print("creating material {}, mat_id {}\n", key, material_id);
    }

    std::vector<PbrMaterial::PbrConstants> material_constants(getMaxMaterials());

    PbrMaterial::PbrConstants* mappedData;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(),
                          repository_->material_data.constants_buffer.allocation,
                          (void**)&mappedData));
    memcpy(mappedData, material_constants.data(),
           sizeof(PbrMaterial::PbrConstants) * getMaxMaterials());

    vmaUnmapMemory(gpu_->getAllocator(), repository_->material_data.constants_buffer.allocation);

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    for (int i = 0; i < material_constants.size(); ++i) {
      VkDescriptorBufferInfo bufferInfo = {};
      bufferInfo.buffer = repository_->material_data.constants_buffer.buffer;
      bufferInfo.offset = sizeof(PbrMaterial::PbrConstants) * i;
      bufferInfo.range = sizeof(PbrMaterial::PbrConstants);
      bufferInfos.push_back(bufferInfo);
    }

    writer_->clear();
    writer_->write_buffer_array(5, bufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    writer_->update_set(gpu_->getDevice(), repository_->material_data.constants_set);

    writer_->clear();
    std::vector<VkDescriptorImageInfo> imageInfos{getMaxTextures()};
    for (int i = 0; i < getMaxTextures(); ++i) {
      VkDescriptorImageInfo image_info;
      image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_info.imageView = repository_->default_material_.error_checkerboard_image.imageView;
      image_info.sampler = repository_->default_material_.linearSampler;
      imageInfos[i] = image_info;
    }
    writer_->write_image_array(4, imageInfos, 0);
    writer_->update_set(gpu_->getDevice(), repository_->material_data.resource_set);
  }

  void MaterialSystem::prepare() {
    auto& material_data = repository_->material_data;

    material_data.constants_buffer = resource_manager_->create_buffer(
        sizeof(PbrMaterial::PbrConstants) * getMaxMaterials(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    descriptor_layout_builder_->clear();
    material_data.resource_layout = descriptor_layout_builder_
                                        ->add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                        .build(gpu_->getDevice());
    descriptor_layout_builder_->clear();
    material_data.constants_layout = descriptor_layout_builder_
                                         ->add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                       VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                         .build(gpu_->getDevice());
    material_data.resource_set = resource_manager_->get_descriptor_pool()->allocate(
        gpu_->getDevice(), material_data.resource_layout, {getMaxTextures()});
    material_data.constants_set = resource_manager_->get_descriptor_pool()->allocate(
        gpu_->getDevice(), material_data.constants_layout, {getMaxMaterials()});

    create_defaults();

    IblBuffers ibl_data{};

    descriptor_layout_builder_->clear();
    ibl_data.descriptor_layout = descriptor_layout_builder_
                                     ->add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT)
                                     .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT)
                                     .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT)
                                     .build(gpu_->getDevice());

    ibl_data.descriptor_set = resource_manager_->get_descriptor_pool()->allocate(
        gpu_->getDevice(), ibl_data.descriptor_layout);
    repository_->register_buffer(ibl_data);

    resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");
  }

  void MaterialSystem::update() {
    for (size_t i = 0; i < repository_->materials.size(); ++i) {
      auto& [name, config, is_dirty] = repository_->materials.get(i);
      if (is_dirty) {
        write_material(config, i);
        is_dirty = false;
      }
    }
  }

  void MaterialSystem::cleanup() {
    const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
    if (ibl_buffers.descriptor_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(gpu_->getDevice(), ibl_buffers.descriptor_layout, nullptr);
    }
    resource_manager_->destroy_image(ibl_buffers.bdrf_lut);
    resource_manager_->destroy_image(ibl_buffers.environment_irradiance_map);
    resource_manager_->destroy_image(ibl_buffers.environment_map);
    vkDestroySampler(gpu_->getDevice(), ibl_buffers.cube_map_sampler, nullptr);
    repository_->deregister_buffer<IblBuffers>();

    for (auto texture : repository_->textures.data()) {
           resource_manager_->destroy_image(texture);
    }
    repository_->textures.clear();

    repository_->materials.clear();

    const auto& materials = repository_->material_data;
    resource_manager_->destroy_buffer(materials.constants_buffer);
    if (materials.resource_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(gpu_->getDevice(), materials.resource_layout, nullptr);
    }
    if (materials.constants_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(gpu_->getDevice(), materials.constants_layout, nullptr);
    }

    const auto& samplers = repository_->samplers.data();
    for (auto& sampler : samplers) {
      vkDestroySampler(gpu_->getDevice(), sampler, nullptr);
    }

    repository_->samplers.clear();
  }

}  // namespace gestalt::application