#include "MaterialSystem.hpp"

#include "VulkanCheck.hpp"
#include <fmt/core.h>
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"

#include <ranges>

namespace gestalt::application {

  constexpr uint32 kWhite = 0xFFFFFFFF;  // White color for color and occlusion
  constexpr uint32 kDefaultMetallicRoughness
      = 0xFF00FFFF;                           // Green color representing metallic-roughness
  constexpr uint32 kFlatNormal = 0xFFFF8080;  // Flat normal
  constexpr uint32 kBlack = 0xFF000000;       // Black color for emissive
  constexpr uint32 kMagenta = 0xFFFF00FF;     // Magenta color for error textures

  constexpr std::array<unsigned char, 4> kWhiteData = {255, 255, 255, 255};
  constexpr std::array<unsigned char, 4> kDefaultMetallicRoughnessData = {0, 255, 0, 255};
  constexpr std::array<unsigned char, 4> kFlatNormalData = {255, 128, 128, 255};
  constexpr std::array<unsigned char, 4> kBlackData = {0, 0, 0, 255};

  
  MaterialSystem::MaterialSystem(IGpu& gpu, IResourceAllocator& resource_allocator,
                                 Repository& repository, FrameProvider& frame)
      : gpu_(gpu),
        resource_allocator_(resource_allocator),
        repository_(repository),
        frame_(frame) {
    create_buffers();
    create_default_material();
    fill_uniform_buffer();
  }

  void MaterialSystem::create_buffers() {
    const auto& mat_buffers = repository_.material_buffers;

    mat_buffers->material_buffer = resource_allocator_.create_buffer(BufferTemplate(
        "Material Storage Buffer", sizeof(PbrMaterial::PbrConstants) * getMaxMaterials(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
  }

  void MaterialSystem::create_default_material() {


    auto& default_mat = repository_.default_material_;


    default_mat.color_image_instance = resource_allocator_.create_image(
        ImageTemplate("Default Color Image")
        .set_image_size(1, 1).set_image_type(TextureType::kColor, VK_FORMAT_R8G8B8A8_UNORM)
        .set_initial_value(kWhiteData.data(), kWhiteData.size()).build());
    repository_.textures.add(default_mat.color_image_instance);

    default_mat.metallic_roughness_image_instance = resource_allocator_.create_image(
        ImageTemplate("Default Metallic Roughness Image")
        .set_image_size(1, 1).set_image_type(TextureType::kColor, VK_FORMAT_R8G8B8A8_UNORM)
        .set_initial_value(kDefaultMetallicRoughnessData.data(),
                           kDefaultMetallicRoughnessData.size()).build());
    repository_.textures.add(default_mat.metallic_roughness_image_instance);

    default_mat.normal_image_instance = resource_allocator_.create_image(
        ImageTemplate("Default Normal Image")
        .set_image_size(1, 1).set_image_type(TextureType::kColor, VK_FORMAT_R8G8B8A8_UNORM)
        .set_initial_value(kFlatNormalData.data(), kFlatNormalData.size()).build());
    repository_.textures.add(default_mat.normal_image_instance);

    default_mat.emissive_image_instance = resource_allocator_.create_image(
        ImageTemplate("Default Emissive Image")
        .set_image_size(1, 1).set_image_type(TextureType::kColor, VK_FORMAT_R8G8B8A8_UNORM)
        .set_initial_value(kBlackData.data(), kBlackData.size()).build());
    repository_.textures.add(default_mat.emissive_image_instance);

    default_mat.occlusion_image_instance = resource_allocator_.create_image(
        ImageTemplate("Default Occlusion Image")
        .set_image_size(1, 1).set_image_type(TextureType::kColor, VK_FORMAT_R8G8B8A8_UNORM)
        .set_initial_value(kWhiteData.data(), kWhiteData.size()).build());
    repository_.textures.add(default_mat.occlusion_image_instance);

    // checkerboard image for error textures and testing
    constexpr size_t checkerboard_size = 256;
    std::array<uint32, checkerboard_size> pixels; // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
        pixels[y * 16 + x] = ((x % 2) ^ (y % 2))
                               ? kMagenta
                               : kBlack;
      }
    }
    constexpr int checkerboard_width = 16;
    constexpr int checkerboard_height = 16;
    constexpr int checkerboard_channels = 4; // RGBA

    std::array<unsigned char, checkerboard_width * checkerboard_height * checkerboard_channels>
        checkerboard; // 16x16 RGBA texture

    for (int y = 0; y < checkerboard_height; y++) {
      for (int x = 0; x < checkerboard_width; x++) {
        int index = (y * checkerboard_width + x) * checkerboard_channels;
        // Index into the array for RGBA
        if ((x % 2) ^ (y % 2)) {
          // Magenta: RGBA = (255, 0, 255, 255)
          checkerboard[index + 0] = 255; // Red
          checkerboard[index + 1] = 0;   // Green
          checkerboard[index + 2] = 255; // Blue
          checkerboard[index + 3] = 255; // Alpha
        } else {
          // Black: RGBA = (0, 0, 0, 255)
          checkerboard[index + 0] = 0;   // Red
          checkerboard[index + 1] = 0;   // Green
          checkerboard[index + 2] = 0;   // Blue
          checkerboard[index + 3] = 255; // Alpha
        }
    }
    }

    default_mat.error_checkerboard_image_instance = resource_allocator_.create_image(
        ImageTemplate("Error Checkerboard Image")
        .set_image_size(16, 16).set_image_type(TextureType::kColor, VK_FORMAT_R8G8B8A8_UNORM)
        .set_initial_value(checkerboard.data(), checkerboard.size()).build());
    repository_.textures.add(default_mat.error_checkerboard_image_instance);

    auto max_sampler_anisotropy = gpu_.getPhysicalDeviceProperties().limits.maxSamplerAnisotropy;

    default_mat.color_sampler = std::make_unique<SamplerInstance>(
        SamplerTemplate("Albedo Sampler", VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE,
                        0.f, 0.0f, VK_LOD_CLAMP_NONE, max_sampler_anisotropy, VK_FALSE, VK_COMPARE_OP_NEVER,
                        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK),
        gpu_.getDevice());       
    default_mat.metallic_roughness_sampler = std::make_unique<SamplerInstance>(
        SamplerTemplate("Metal Rough Sampler", VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE,
                        0.0f, 0.0f, VK_LOD_CLAMP_NONE, max_sampler_anisotropy, VK_FALSE, VK_COMPARE_OP_NEVER,
                        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK),
        gpu_.getDevice());
    default_mat.normal_sampler = std::make_unique<SamplerInstance>(
        SamplerTemplate("NormalMapSampler", VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE,
                        0.0f, 0.0f, VK_LOD_CLAMP_NONE, max_sampler_anisotropy, VK_FALSE, VK_COMPARE_OP_NEVER,
                        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK),
        gpu_.getDevice());
    default_mat.emissive_sampler = std::make_unique<SamplerInstance>(
        SamplerTemplate("EmissiveSampler", VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FALSE,
                        0.0f, 0.0f, VK_LOD_CLAMP_NONE, 1.0f, VK_FALSE, VK_COMPARE_OP_NEVER,
                        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK),
        gpu_.getDevice());
    default_mat.occlusion_sampler = std::make_unique<SamplerInstance>(
        SamplerTemplate("Ao Sampler", VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        VK_FALSE, 
                        0.0f, 0.0f, VK_LOD_CLAMP_NONE, 1.0f, VK_FALSE, VK_COMPARE_OP_NEVER,
                        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK),
        gpu_.getDevice());

    PbrMaterial pbr_mat{};

    // default the material textures
    pbr_mat.textures.albedo_image = default_mat.color_image_instance;
    pbr_mat.textures.albedo_sampler = default_mat.color_sampler->get_sampler_handle();
    pbr_mat.textures.metal_rough_image = default_mat.metallic_roughness_image_instance;
    pbr_mat.textures.metal_rough_sampler
        = default_mat.metallic_roughness_sampler->get_sampler_handle();
    pbr_mat.textures.normal_image = default_mat.normal_image_instance;
    pbr_mat.textures.normal_sampler = default_mat.normal_sampler->get_sampler_handle();
    pbr_mat.textures.emissive_image = default_mat.emissive_image_instance;
    pbr_mat.textures.emissive_sampler = default_mat.emissive_sampler->get_sampler_handle();
    pbr_mat.textures.occlusion_image = default_mat.occlusion_image_instance;
    pbr_mat.textures.occlusion_sampler = default_mat.occlusion_sampler->get_sampler_handle();

    // build material
    const size_t material_id = repository_.materials.size();
    const std::string key = "default_material";
    repository_.materials.add(Material{.name = key, .config = pbr_mat});
    fmt::print("creating material {}, mat_id {}\n", key, material_id);
  }

  void MaterialSystem::fill_uniform_buffer() {
    const auto& constant_buffer = repository_.material_buffers->material_buffer;

    const std::vector<PbrMaterial::PbrConstants> material_constants(getMaxMaterials());

    PbrMaterial::PbrConstants* uniformBufferPointer;
    VK_CHECK(vmaMapMemory(gpu_.getAllocator(), constant_buffer->get_allocation(),
                          (void**)&uniformBufferPointer));
    memcpy(uniformBufferPointer, material_constants.data(),
           sizeof(PbrMaterial::PbrConstants) * getMaxMaterials());

    vmaUnmapMemory(gpu_.getAllocator(), constant_buffer->get_allocation());
  }

  void MaterialSystem::update() {
    bool first_processed = false;

    for (size_t material_id = 0; material_id < repository_.materials.size(); ++material_id) {
      auto& [name, config, is_dirty] = repository_.materials.get(material_id);
      if (is_dirty) {
        if (!first_processed) {
          first_processed = true;
          vkDeviceWaitIdle(gpu_.getDevice());
        }

        write_material(config, static_cast<uint32>(material_id));
        is_dirty = false;
      }
    }

    if (first_processed) {
      // TODO batch update textures
    }

  }
  
  MaterialSystem::~MaterialSystem() {
    const auto& material_buffers = repository_.material_buffers;
    repository_.materials.clear();
    resource_allocator_.destroy_buffer(material_buffers->material_buffer);
  }

  void MaterialSystem::write_material(PbrMaterial& material, const uint32 material_id) {
    if (material_id >= getMaxMaterials()) {
      throw std::runtime_error("Material ID exceeds maximum materials");
    }

    // Calculate the start offset in the descriptor buffer
    const uint32_t texture_start = getPbrMaterialTextures() * material_id;

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
    vmaMapMemory(gpu_.getAllocator(), repository_.material_buffers->material_buffer->get_allocation(),
                 (void**)&mappedData);
    memcpy(mappedData + material_id, &material.constants, sizeof(PbrMaterial::PbrConstants));
    vmaUnmapMemory(gpu_.getAllocator(), repository_.material_buffers->material_buffer->get_allocation());
  }

}  // namespace gestalt::application