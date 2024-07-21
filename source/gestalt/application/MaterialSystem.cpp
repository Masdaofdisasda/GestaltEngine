#include "SceneSystem.hpp"

namespace gestalt::application {

  void MaterialSystem::prepare() {
    repository_->material_buffers->constants_buffer = resource_manager_->create_buffer(
        sizeof(PbrMaterial::PbrConstants) * getMaxMaterials(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    descriptor_layout_builder_->clear();
    const auto texture_layout = descriptor_layout_builder_
                                    ->add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                    .build(gpu_->getDevice());
    descriptor_layout_builder_->clear();
    const auto constants_layout = descriptor_layout_builder_
                                      ->add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                    VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                      .build(gpu_->getDevice());
    repository_->material_buffers->texture_set
        = resource_manager_->allocateDescriptor(texture_layout, {getMaxTextures()});
    repository_->material_buffers->constants_set
        = resource_manager_->allocateDescriptor(constants_layout, {getMaxMaterials()});

    create_defaults();

    descriptor_layout_builder_->clear();
    const auto descriptor_layout = descriptor_layout_builder_
                                       ->add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                     VK_SHADER_STAGE_FRAGMENT_BIT)
                                       .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    VK_SHADER_STAGE_FRAGMENT_BIT)
                                       .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    VK_SHADER_STAGE_FRAGMENT_BIT)
                                       .build(gpu_->getDevice());

    repository_->ibl_buffers->descriptor_set
        = resource_manager_->allocateDescriptor(descriptor_layout);

    resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");

    vkDestroyDescriptorSetLayout(gpu_->getDevice(), texture_layout, nullptr);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), constants_layout, nullptr);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);
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
      repository_->materials.add(Material{.name = key, .config = pbr_material});
      fmt::print("creating material {}, mat_id {}\n", key, material_id);
    }

    std::vector<PbrMaterial::PbrConstants> material_constants(getMaxMaterials());

    PbrMaterial::PbrConstants* mappedData;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(),
                          repository_->material_buffers->constants_buffer->allocation,
                          (void**)&mappedData));
    memcpy(mappedData, material_constants.data(),
           sizeof(PbrMaterial::PbrConstants) * getMaxMaterials());

    vmaUnmapMemory(gpu_->getAllocator(),
                   repository_->material_buffers->constants_buffer->allocation);

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    for (int i = 0; i < material_constants.size(); ++i) {
      VkDescriptorBufferInfo bufferInfo = {};
      bufferInfo.buffer = repository_->material_buffers->constants_buffer->buffer;
      bufferInfo.offset = sizeof(PbrMaterial::PbrConstants) * i;
      bufferInfo.range = sizeof(PbrMaterial::PbrConstants);
      bufferInfos.push_back(bufferInfo);
    }

    writer_->clear();
    writer_->write_buffer_array(5, bufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    writer_->update_set(gpu_->getDevice(), repository_->material_buffers->constants_set);

    writer_->clear();
    for (int i = 0; i < getMaxMaterials(); ++i) {
      image_infos_[i]
          = {{{default_material.linearSampler, default_material.color_image.imageView,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
              {default_material.linearSampler, default_material.metallic_roughness_image.imageView,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
              {default_material.linearSampler, default_material.normal_image.imageView,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
              {default_material.linearSampler, default_material.emissive_image.imageView,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
              {default_material.nearestSampler, default_material.occlusion_image.imageView,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}};

      writer_->write_image_array(4, image_infos_[i], i * getPbrMaterialTextures());
    }
    writer_->update_set(gpu_->getDevice(), repository_->material_buffers->texture_set);
  }

  void MaterialSystem::update() {
    bool first_processed = false;
    writer_->clear();

    for (size_t i = 0; i < repository_->materials.size(); ++i) {
      auto& [name, config, is_dirty] = repository_->materials.get(i);
      if (is_dirty) {
        if (!first_processed) {
          first_processed = true;
        }

        write_material(config, i);
        is_dirty = false;
      }
    }

    if (first_processed) {
      vkDeviceWaitIdle(gpu_->getDevice());
      writer_->update_set(gpu_->getDevice(), repository_->material_buffers->constants_set);
    }
  }

  void MaterialSystem::cleanup() {
    const auto& ibl_buffers = repository_->ibl_buffers;
    resource_manager_->destroy_image(ibl_buffers->bdrf_lut);
    resource_manager_->destroy_image(ibl_buffers->environment_irradiance_map);
    resource_manager_->destroy_image(ibl_buffers->environment_map);
    vkDestroySampler(gpu_->getDevice(), ibl_buffers->cube_map_sampler, nullptr);

    for (auto texture : repository_->textures.data()) {
      resource_manager_->destroy_image(texture);
    }
    repository_->textures.clear();

    repository_->materials.clear();

    resource_manager_->destroy_buffer(repository_->material_buffers->constants_buffer);

    const auto& samplers = repository_->samplers.data();
    for (auto& sampler : samplers) {
      vkDestroySampler(gpu_->getDevice(), sampler, nullptr);
    }

    repository_->samplers.clear();
  }

  // TODO likely there is no sync between subsequent descriptor set updates
  void MaterialSystem::write_material(PbrMaterial& material, const uint32 material_id) {
    assert(material_id < getMaxMaterials());

    image_infos_[material_id]
        = {{{material.textures.albedo_sampler, material.textures.albedo_image.imageView,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {material.textures.metal_rough_sampler, material.textures.metal_rough_image.imageView,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {material.textures.normal_sampler, material.textures.normal_image.imageView,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {material.textures.emissive_sampler, material.textures.emissive_image.imageView,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {material.textures.occlusion_sampler, material.textures.occlusion_image.imageView,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}};

    const uint32 texture_start = getPbrMaterialTextures() * material_id;
    writer_->write_image_array(4, image_infos_[material_id], texture_start);

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
    vmaMapMemory(gpu_->getAllocator(), repository_->material_buffers->constants_buffer->allocation,
                 (void**)&mappedData);
    memcpy(mappedData + material_id, &material.constants, sizeof(PbrMaterial::PbrConstants));
    vmaUnmapMemory(gpu_->getAllocator(),
                   repository_->material_buffers->constants_buffer->allocation);
  }

}  // namespace gestalt::application