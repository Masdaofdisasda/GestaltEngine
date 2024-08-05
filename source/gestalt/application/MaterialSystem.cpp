#include "SceneSystem.hpp"
#include "descriptor.hpp"

namespace gestalt::application {

  constexpr uint32 kWhite = 0xFFFFFFFF;  // White color for color and occlusion
  constexpr uint32 kDefaultMetallicRoughness
      = 0xFF00FFFF;                           // Green color representing metallic-roughness
  constexpr uint32 kFlatNormal = 0xFFFF8080;  // Flat normal
  constexpr uint32 kBlack = 0xFF000000;       // Black color for emissive
  constexpr uint32 kMagenta = 0xFFFF00FF;     // Magenta color for error textures

  void MaterialSystem::prepare() {
    create_uniform_buffer();
    create_images_buffer();

    create_default_material();

    fill_uniform_buffer();
    fill_images_buffer();

    create_ibl_images();
  }

  void MaterialSystem::create_uniform_buffer() {
    const auto& mat_buffers = repository_->material_buffers;

    descriptor_layout_builder_->clear();
    const auto uniformLayout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false, getMaxMaterials())
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mat_buffers->uniform_descriptor_buffer
        = resource_manager_->create_descriptor_buffer(uniformLayout, 1);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), uniformLayout, nullptr);

    mat_buffers->uniform_buffer = resource_manager_->create_buffer(
        sizeof(PbrMaterial::PbrConstants) * getMaxMaterials(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  void MaterialSystem::create_images_buffer() {
    const auto& mat_buffers = repository_->material_buffers;

    descriptor_layout_builder_->clear();
    const auto imageLayout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_FRAGMENT_BIT, false, getMaxTextures())
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    repository_->material_buffers->image_descriptor_buffer
        = resource_manager_->create_descriptor_buffer(
            imageLayout, 1, VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), imageLayout, nullptr);
  }

  void MaterialSystem::create_ibl_images() {
    descriptor_layout_builder_->clear();
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    repository_->ibl_buffers->descriptor_buffer = resource_manager_->create_descriptor_buffer(
        descriptor_layout, 3, VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");
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

    VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .magFilter = VK_FILTER_NEAREST,
                                   .minFilter = VK_FILTER_NEAREST};
    vkCreateSampler(gpu_->getDevice(), &sampler, nullptr, &default_mat.nearestSampler);
    repository_->samplers.add(default_mat.nearestSampler);

    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(gpu_->getDevice(), &sampler, nullptr, &default_mat.linearSampler);
    repository_->samplers.add(default_mat.linearSampler);

    PbrMaterial pbr_mat{};

    // default the material textures
    pbr_mat.textures.albedo_image = default_mat.color_image;
    pbr_mat.textures.albedo_sampler = default_mat.linearSampler;
    pbr_mat.textures.metal_rough_image = default_mat.metallic_roughness_image;
    pbr_mat.textures.metal_rough_sampler = default_mat.linearSampler;
    pbr_mat.textures.normal_image = default_mat.normal_image;
    pbr_mat.textures.normal_sampler = default_mat.linearSampler;
    pbr_mat.textures.emissive_image = default_mat.emissive_image;
    pbr_mat.textures.emissive_sampler = default_mat.linearSampler;
    pbr_mat.textures.occlusion_image = default_mat.occlusion_image;
    pbr_mat.textures.occlusion_sampler = default_mat.nearestSampler;

    // build material
    const size_t material_id = repository_->materials.size();
    const std::string key = "default_material";
    repository_->materials.add(Material{.name = key, .config = pbr_mat});
    fmt::print("creating material {}, mat_id {}\n", key, material_id);
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

    const auto& descriptor_buffer = repository_->material_buffers->uniform_descriptor_buffer;

    descriptor_buffer
        ->write_buffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, constant_buffer->address,
                             sizeof(PbrMaterial::PbrConstants) * getMaxMaterials())
        .update();
  }

  void MaterialSystem::fill_images_buffer() {
    const auto& mat_buffers = repository_->material_buffers;
    const auto& default_mat = repository_->default_material_;

    std::vector<VkDescriptorImageInfo> image_infos;

    for (int i = 0; i < getMaxTextures(); ++i) {
      image_infos.push_back({default_mat.linearSampler, default_mat.color_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.linearSampler,
                             default_mat.metallic_roughness_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.linearSampler, default_mat.normal_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.linearSampler, default_mat.emissive_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      image_infos.push_back({default_mat.nearestSampler, default_mat.occlusion_image.imageView,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }

    mat_buffers->image_descriptor_buffer
        ->write_image_array(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_infos, 0)
        .update();
  }

  void MaterialSystem::update() {
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

    resource_manager_->destroy_buffer(repository_->material_buffers->uniform_buffer);

    const auto& samplers = repository_->samplers.data();
    for (auto& sampler : samplers) {
      vkDestroySampler(gpu_->getDevice(), sampler, nullptr);
    }

    repository_->samplers.clear();
  }

  // TODO likely there is no sync between subsequent descriptor set updates
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

    mat_buffers->image_descriptor_buffer->update();

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