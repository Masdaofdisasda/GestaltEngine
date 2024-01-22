
#include <stb_image.h>
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include "camera.h"

VkFilter extract_filter(fastgltf::Filter filter) {
  switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
      return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return VK_FILTER_LINEAR;
  }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter) {
  switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

std::optional<AllocatedImage> load_image(render_engine* engine, fastgltf::Asset& asset,
                                         fastgltf::Image& image) {
  AllocatedImage newImage{};

  int width, height, nrChannels;

  std::visit(fastgltf::visitor{
                 [](auto& arg) {},
                 [&](fastgltf::sources::URI& filePath) {
                   assert(filePath.fileByteOffset == 0);  // We don't support offsets with stbi.
                   assert(filePath.uri.isLocalPath());    // We're only capable of loading
                                                          // local files.

                   const std::string path(filePath.uri.path().begin(),
                                          filePath.uri.path().end());  // Thanks C++.
                   unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                   if (data) {
                     VkExtent3D imagesize;
                     imagesize.width = width;
                     imagesize.height = height;
                     imagesize.depth = 1;

                     newImage = engine->get_resource_manager().create_image(
                         data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_IMAGE_USAGE_SAMPLED_BIT, true);

                     stbi_image_free(data);
                   }
                 },
                 [&](fastgltf::sources::Vector& vector) {
                   unsigned char* data = stbi_load_from_memory(
                       vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height,
                       &nrChannels, 4);
                   if (data) {
                     VkExtent3D imagesize;
                     imagesize.width = width;
                     imagesize.height = height;
                     imagesize.depth = 1;

                     newImage = engine->get_resource_manager().create_image(
                         data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_IMAGE_USAGE_SAMPLED_BIT, true);

                     stbi_image_free(data);
                   }
                 },
                 [&](fastgltf::sources::BufferView& view) {
                   auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                   auto& buffer = asset.buffers[bufferView.bufferIndex];

                   std::visit(fastgltf::visitor{// We only care about VectorWithMime here, because
                                                // we specify LoadExternalBuffers, meaning all
                                                // buffers are already loaded into a vector.
                                                [](auto& arg) {},
                                                [&](fastgltf::sources::Vector& vector) {
                                                  unsigned char* data = stbi_load_from_memory(
                                                      vector.bytes.data() + bufferView.byteOffset,
                                                      static_cast<int>(bufferView.byteLength),
                                                      &width, &height, &nrChannels, 4);
                                                  if (data) {
                                                    VkExtent3D imagesize;
                                                    imagesize.width = width;
                                                    imagesize.height = height;
                                                    imagesize.depth = 1;

                                                    newImage
                                                        = engine->get_resource_manager()
                                                              .create_image(
                                                        data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                        VK_IMAGE_USAGE_SAMPLED_BIT, true);

                                                    stbi_image_free(data);
                                                  }
                                                }},
                              buffer.data);
                 },
             },
             image.data);

  // if any of the attempts to load the data failed, we havent written the image
  // so handle is null
  if (newImage.image == VK_NULL_HANDLE) {
    return {};
  }
  return newImage;
}

void vk_scene_manager::load_scene_from_gltf(const std::string& filename) {

  //TODO load from file and parse

  std::vector<Vertex> vertices{4};
  vertices[0].position = {-1.f, -1.f, 0.f};
  vertices[0].color = glm::vec4(1.f);
  vertices[0].normal = {0.f, 0.f, 1.f};
  vertices[0].uv_x = 0.f;
  vertices[0].uv_y = 0.f;
  vertices[1].position = {1.f, -1.f, 0.f};
  vertices[1].color = glm::vec4(1.f);
  vertices[1].normal = {0.f, 0.f, 1.f};
  vertices[1].uv_x = 1.f;
  vertices[1].uv_y = 0.f;
  vertices[2].position = {1.f, 1.f, 0.f};
  vertices[2].color = glm::vec4(1.f);
  vertices[2].normal = {0.f, 0.f, 1.f};
  vertices[2].uv_x = 1.f;
  vertices[2].uv_y = 1.f;
  vertices[3].position = {-1.f, 1.f, 0.f};
  vertices[3].color = glm::vec4(1.f);
  vertices[3].normal = {0.f, 0.f, 1.f};
  vertices[3].uv_x = 0.f;
  vertices[3].uv_y = 1.f;

  std::vector<uint32_t> indices = {
      0, 1, 2, 2, 3, 0,
  };

  constexpr int steps = 10;
  std::array<uint32_t, steps * steps> pixels;
  std::array<uint32_t, steps * steps * steps> albedo;

  for (int i = 0; i < steps; ++i) {
    for (int j = 0; j < steps; ++j) {
      int metallicValue = (i * 255) / 9;
      int roughnessValue = (j * 255) / 9;
      pixels[i * steps + j] = (255 << 24) | (roughnessValue << 8) | metallicValue;
      for (int k = 0; k < steps; ++k) {
               albedo[i * steps * steps + j * steps + k] = (255 << 24) | (i * 255 / 9 << 16)
                                                    | (j * 255 / 9 << 8) | (k * 255 / 9);
      }
    }
  }

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      for (int k = 0; k < 10; ++k) {
        entity entity = create_entity();  // todo maybe use builder pattern?
        add_mesh_component(entity, vertices, indices);
        add_transform_component(entity, glm::vec3(i - 5.f, j - 5.f, k -5.f),
                                glm::quat(0.f, 0.f, 0.f, 0.f), glm::vec3(0.4f));
        root_.children.push_back(entity);

        gltf_metallic_roughness::MaterialResources resources;
        {
          struct material {
            AllocatedImage color_image;
            AllocatedImage metallic_roughness_image;
            AllocatedImage normal_image;
            AllocatedImage emissive_image;
            AllocatedImage occlusion_image;

            AllocatedImage error_checkerboard_image;

            VkSampler default_sampler_linear;
            VkSampler default_sampler_nearest;
          } default_material;

          uint32_t white = albedo[i * steps * steps + j * steps + k];
          uint32_t occlusion = 0xFFFFFFFF;  // White color for color and occlusion

          uint32_t default_metallic_roughness = pixels[i * steps + j];
          uint32_t flat_normal = 0xFFFF8080;  // Flat normal
          uint32_t black = 0xFF000000;        // Black color for emissive

          default_material.color_image = resource_manager_.create_image(
              (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
              VK_IMAGE_USAGE_SAMPLED_BIT);

          default_material.metallic_roughness_image = resource_manager_.create_image(
              (void*)&default_metallic_roughness, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
              VK_IMAGE_USAGE_SAMPLED_BIT);

          default_material.normal_image = resource_manager_.create_image(
              (void*)&flat_normal, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
              VK_IMAGE_USAGE_SAMPLED_BIT);

          default_material.emissive_image = resource_manager_.create_image(
              (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
              VK_IMAGE_USAGE_SAMPLED_BIT);

          default_material.occlusion_image = resource_manager_.create_image(
              (void*)&occlusion, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
              VK_IMAGE_USAGE_SAMPLED_BIT);

          VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

          sampl.magFilter = VK_FILTER_NEAREST;
          sampl.minFilter = VK_FILTER_NEAREST;

          vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material.default_sampler_nearest);

          sampl.magFilter = VK_FILTER_LINEAR;
          sampl.minFilter = VK_FILTER_LINEAR;
          vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material.default_sampler_linear);

          // Create buffer for material data
          AllocatedBuffer materialDataBuffer = resource_manager_.create_buffer(
              sizeof(gltf_metallic_roughness::MaterialConstants),
              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

          gltf_metallic_roughness::MaterialConstants constants;
          constants.colorFactors.x = 1.f;
          constants.colorFactors.y = 1.f;
          constants.colorFactors.z = 1.f;
          constants.colorFactors.w = 1.f;

          constants.metal_rough_factors.x = 0.f;
          constants.metal_rough_factors.y = 0.f;
          // Map and write material parameters to buffer
          gltf_metallic_roughness::MaterialConstants* sceneMaterialConstants
              = (gltf_metallic_roughness::MaterialConstants*)materialDataBuffer.info.pMappedData;
          *sceneMaterialConstants = constants;

          // Create material resources

          resources.colorImage = default_material.color_image;
          resources.colorSampler = default_material.default_sampler_linear;
          resources.metalRoughImage = default_material.metallic_roughness_image;
          resources.metalRoughSampler = default_material.default_sampler_linear;
          resources.normalImage = default_material.normal_image;
          resources.normalSampler = default_material.default_sampler_linear;
          resources.emissiveImage = default_material.emissive_image;
          resources.emissiveSampler = default_material.default_sampler_linear;
          resources.occlusionImage = default_material.occlusion_image;
          resources.occlusionSampler = default_material.default_sampler_nearest;

          resources.dataBuffer = materialDataBuffer.buffer;
          resources.dataBufferOffset = 0;  // Assuming only one material constants struct
        }
        // load textures and so on
        add_material(MaterialPass::MainColor, resources,
                     "mat_" + std::to_string(i) + "_" + std::to_string(j));
        add_material_component(entity, "mat_" + std::to_string(i) + "_" + std::to_string(j));
      }
    }
  }

  // after all meshes are loaded:
  mesh_buffers_ = resource_manager_.upload_mesh(indices_, vertices_);

}

void vk_scene_manager::add_transform_component(entity entity, const glm::vec3& position,
                                               const glm::quat& rotation, const glm::vec3& scale) {
  const transform_component transform{position, rotation, scale};
  transforms_.push_back(transform);
  scene_object& object = entity_hierarchy_[entity];
  object.transform = transforms_.size() - 1;
}

void vk_scene_manager::add_mesh_component(const entity entity, std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices) {

  mesh_component mesh;
  mesh.vertex_offset = vertices_.size();
  mesh.first_index = indices_.size();
  mesh.vertex_count = vertices.size();
  mesh.index_count = indices.size();

  glm::vec3 minpos = vertices[0].position;
  glm::vec3 maxpos = vertices[0].position;
  for (auto& vertice : vertices) {
    minpos = min(minpos, vertice.position);
    maxpos = max(maxpos, vertice.position);
  }

  mesh.bounds.origin = (maxpos + minpos) / 2.f;
  mesh.bounds.extents = (maxpos - minpos) / 2.f;
  mesh.bounds.sphereRadius = length(mesh.bounds.extents);

  vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
  indices_.insert(indices_.end(), indices.begin(), indices.end());

  meshes_.push_back(mesh);
  scene_object& object = entity_hierarchy_[entity];
  object.mesh = meshes_.size() - 1;
}

const std::vector<entity>& vk_scene_manager::get_children(entity entity) const {
  return entity_hierarchy_.at(entity).children;
}

void vk_scene_manager::update_scene(draw_context& draw_context) {

  // get root
  for (const auto& nodes = root_.children; auto& entity : nodes) {

    const auto& object = entity_hierarchy_.at(entity);
    const auto& mesh = meshes_.at(object.mesh);
    const auto& transform = transforms_.at(object.transform);
    auto& material = materials_.at(object.material);

    render_object def;
    def.index_count = mesh.index_count;
    def.first_index = mesh.first_index;
    def.index_buffer = mesh_buffers_.indexBuffer.buffer;
    def.material = &material.data;
    def.bounds = mesh.bounds;
    def.transform = transform.getModelMatrix();
    def.vertex_buffer_address = mesh_buffers_.vertexBufferAddress;

    draw_context.opaque_surfaces.push_back(def);
  }
  // repeat for all child nodes
}


void vk_scene_manager::init_default_data() {

  struct material {
    AllocatedImage color_image;
    AllocatedImage metallic_roughness_image;
    AllocatedImage normal_image;
    AllocatedImage emissive_image;
    AllocatedImage occlusion_image;

    AllocatedImage error_checkerboard_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;
  } default_material;

  
  uint32_t white = 0xFFFFFFFF;                            // White color for color and occlusion
  uint32_t default_metallic_roughness = 0xFF00FF00;     // Green color representing metallic-roughness
  uint32_t flat_normal = 0xFFFF8080;                    // Flat normal
  uint32_t black = 0xFF000000;                          // Black color for emissive

  default_material.color_image = resource_manager_.create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.metallic_roughness_image
      = resource_manager_.create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.normal_image
      = resource_manager_.create_image((void*)&flat_normal, VkExtent3D{1, 1, 1},
                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.emissive_image = resource_manager_.create_image(
      (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.occlusion_image = resource_manager_.create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  // checkerboard image for error textures and testing
  uint32_t magenta = 0xFFFF00FF;
  constexpr size_t checkerboard_size = 256;
  std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  default_material.error_checkerboard_image = resource_manager_.create_image(
      pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;

  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material.default_sampler_nearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material.default_sampler_linear);

  // create default material
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
      = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

  descriptorPool.init(gpu_.device, 1, sizes);
  AllocatedBuffer materialDataBuffer = resource_manager_.create_buffer(
      sizeof(gltf_metallic_roughness::MaterialConstants),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  gltf_metallic_roughness::MaterialConstants* sceneMaterialConstants
      = (gltf_metallic_roughness::MaterialConstants*)materialDataBuffer.info.pMappedData;

  gltf_metallic_roughness::MaterialConstants constants;
  constants.colorFactors.x = 1.f;
  constants.colorFactors.y = 1.f;
  constants.colorFactors.z = 1.f;
  constants.colorFactors.w = 1.f;

  constants.metal_rough_factors.x = 0.f;
  constants.metal_rough_factors.y = 0.f;
  // write material parameters to buffer
  sceneMaterialConstants[data_index] = constants;

auto pass_type = MaterialPass::MainColor;

  gltf_metallic_roughness::MaterialResources material_resources;
  // default the material textures
  material_resources.colorImage = default_material.color_image;
  material_resources.colorSampler = default_material.default_sampler_linear;
  material_resources.metalRoughImage = default_material.metallic_roughness_image;
  material_resources.metalRoughSampler = default_material.default_sampler_linear;
  material_resources.normalImage = default_material.normal_image;
  material_resources.normalSampler = default_material.default_sampler_linear;
  material_resources.emissiveImage = default_material.emissive_image;
  material_resources.emissiveSampler = default_material.default_sampler_linear;
  material_resources.occlusionImage = default_material.occlusion_image;
  material_resources.occlusionSampler = default_material.default_sampler_nearest;

  // set the uniform buffer for the material data
  material_resources.dataBuffer = materialDataBuffer.buffer;
  material_resources.dataBufferOffset
      = data_index * sizeof(gltf_metallic_roughness::MaterialConstants);

  // build material
  add_material(pass_type, material_resources, default_material_name_);

  deletion_service_.push_function(
      [this, default_material]() { resource_manager_.destroy_image(default_material.color_image); }); 
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_.destroy_image(default_material.metallic_roughness_image); }); 
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_.destroy_image(default_material.normal_image); }); 
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_.destroy_image(default_material.emissive_image); }); 
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_.destroy_image(default_material.occlusion_image); }); 
  deletion_service_.push(default_material.default_sampler_nearest);
  deletion_service_.push(default_material.default_sampler_linear);
}