
#include <vk_scene_manager.h>

#include "vk_engine.h"
#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include <glm/gtx/matrix_decompose.hpp>

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

void vk_scene_manager::init(const vk_gpu& gpu, const resource_manager& resource_manager,
                            const gltf_metallic_roughness& material) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;
  deletion_service_.init(gpu.device, gpu.allocator);
  gltf_material_ = material;

  init_default_data();
  load_scene_from_gltf(R"(..\..\assets\Models\MetalRoughSpheres\glTF-Binary\MetalRoughSpheres.glb)");
  //load_scene_from_gltf();
}

void vk_scene_manager::cleanup() {
  deletion_service_.flush();
}

void vk_scene_manager::load_scene_from_gltf(const std::string& file_path) {

  fmt::print("Loading GLTF: {}", file_path);

  fastgltf::Parser parser{};

  constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember
                               | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers
                               | fastgltf::Options::LoadExternalBuffers;
  // fastgltf::Options::LoadExternalImages;
  // Options::DecomposeNodeMatrices.

  fastgltf::GltfDataBuffer data;
  data.loadFromFile(file_path);

  fastgltf::Asset gltf;

  // get file type
  std::filesystem::path path = file_path;

  if (auto type = determineGltfFileType(&data); type == fastgltf::GltfType::glTF) {
    auto load = parser.loadGLTF(&data, path.parent_path(), gltf_options);
    if (load) {
      gltf = std::move(load.get());
    } else {
      fmt::print("Failed to load glTF: {}", to_underlying(load.error()));
    }
  } else if (type == fastgltf::GltfType::GLB) {
    auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltf_options);
    if (load) {
      gltf = std::move(load.get());
    } else {
      fmt::print("Failed to load glTF: {}", to_underlying(load.error()));
    }
  } else {
    fmt::print("Failed to determine glTF container");
  }
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
      = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

  descriptorPool.init(gpu_.device, gltf.materials.size(), sizes);

  // samplers and textures
  for (fastgltf::Sampler& sampler : gltf.samplers) {
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.minLod = 0;

    sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
    sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    VkSampler newSampler;
    vkCreateSampler(gpu_.device, &sampl, nullptr, &newSampler);

    samplers_.push_back(newSampler);
  }

  for (fastgltf::Image& image : gltf.images) {
    std::optional<AllocatedImage> img = resource_manager_.load_image(gltf, image);

    if (img.has_value()) {
      images_.push_back(*img);
    } else {
      fmt::print("gltf failed to load texture {}\n", image.name);
    }
  }

  // load buffer and materials
  material_data_buffer_ = resource_manager_.create_buffer(
      sizeof(gltf_metallic_roughness::MaterialConstants) * gltf.materials.size(),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  for (fastgltf::Material& mat : gltf.materials) {

    gltf_metallic_roughness::MaterialConstants constants;
    constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
    constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
    constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
    constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

    constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
    constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
    // write material parameters to buffer
    //sceneMaterialConstants[data_index] = constants; //TODO

    auto pass_type = MaterialPass::MainColor;
    if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
      pass_type = MaterialPass::Transparent;
    }

    gltf_metallic_roughness::MaterialResources materialResources
        = {.colorImage = default_material_.color_image,
           .colorSampler = default_material_.default_sampler_linear,
           .metalRoughImage = default_material_.metallic_roughness_image,
           .metalRoughSampler = default_material_.default_sampler_linear,
           .normalImage = default_material_.normal_image,
           .normalSampler = default_material_.default_sampler_linear,
           .emissiveImage = default_material_.emissive_image,
           .emissiveSampler = default_material_.default_sampler_linear,
           .occlusionImage = default_material_.occlusion_image,
           .occlusionSampler = default_material_.default_sampler_nearest};

    // set the uniform buffer for the material data
    materialResources.dataBuffer = material_data_buffer_.buffer;
    materialResources.dataBufferOffset
        = data_index * sizeof(gltf_metallic_roughness::MaterialConstants);
    // grab textures from gltf file
    if (mat.pbrData.baseColorTexture.has_value()) {
      size_t img
          = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
      size_t sampler
          = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

      materialResources.colorImage = images_[img];
      materialResources.colorSampler = samplers_[sampler];
    }
    if (mat.pbrData.metallicRoughnessTexture) {
      size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex]
                       .imageIndex.value();
      size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex]
                           .samplerIndex.value();

      materialResources.metalRoughImage = images_[img];
      materialResources.metalRoughSampler = samplers_[sampler];
    }
    if (mat.normalTexture.has_value()) {
      size_t img = gltf.textures[mat.normalTexture.value().textureIndex]
                       .imageIndex.value();
      size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex]
                           .samplerIndex.value();

      materialResources.normalImage = images_[img];
      materialResources.normalSampler = samplers_[sampler];
    }
    if (mat.emissiveTexture.has_value()) {
      size_t img = gltf.textures[mat.emissiveTexture.value().textureIndex]
                       .imageIndex.value();
      size_t sampler = gltf.textures[mat.emissiveTexture.value().textureIndex]
                           .samplerIndex.value();

      materialResources.emissiveImage = images_[img];
      materialResources.emissiveSampler = samplers_[sampler];
    }
    if (mat.occlusionTexture.has_value()) {
      size_t img = gltf.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
      size_t sampler
          = gltf.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();

      materialResources.occlusionImage = images_[img];
      materialResources.occlusionSampler = samplers_[sampler];
    }
    // build material
    add_material(pass_type, materialResources, std::string(mat.name));
    data_index++;
  }


  // load meshes
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  size_t node_offset = entity_hierarchy_.size() + 1; // for root
  size_t mesh_offset = meshes_.size();

  for (fastgltf::Mesh& mesh : gltf.meshes) {

    // clear the mesh arrays each mesh, we dont want to merge them by error
    indices.clear();
    vertices.clear();

    for (auto&& primitive : mesh.primitives) {
      size_t initial_vtx = vertices.size();

      // load indexes
      {
        fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
            gltf, indexaccessor, [&](std::uint32_t idx) { indices.push_back(idx + initial_vtx); });
      }

      // load vertex positions
      {
        fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                      [&](glm::vec3 v, size_t index) {
                                                        Vertex newvtx;
                                                        newvtx.position = v;
                                                        newvtx.normal = {1, 0, 0};
                                                        newvtx.color = glm::vec4{1.f};
                                                        newvtx.uv_x = 0;
                                                        newvtx.uv_y = 0;
                                                        vertices[initial_vtx + index] = newvtx;
                                                      });
      }

      // load vertex normals
      auto normals = primitive.findAttribute("NORMAL");
      if (normals != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, gltf.accessors[(*normals).second],
            [&](glm::vec3 v, size_t index) { vertices[initial_vtx + index].normal = v; });
      }

      // load UVs
      auto uv = primitive.findAttribute("TEXCOORD_0");
      if (uv != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                                                      [&](glm::vec2 v, size_t index) {
                                                        vertices[initial_vtx + index].uv_x = v.x;
                                                        vertices[initial_vtx + index].uv_y = v.y;
                                                      });
      }

      // load vertex colors
      auto colors = primitive.findAttribute("COLOR_0");
      if (colors != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            gltf, gltf.accessors[(*colors).second],
            [&](glm::vec4 v, size_t index) { vertices[initial_vtx + index].color = v; });
      }

      entity new_entity = create_entity();
      add_mesh_component(new_entity, vertices, indices, std::string(mesh.name));

      if (primitive.materialIndex.has_value()) {
        auto material_name = gltf.materials[primitive.materialIndex.value()].name;
        add_material_component(new_entity, std::string(material_name));

      } else {
        add_material_component(new_entity, default_material_name_);
      }

      glm::vec3 minpos = vertices[initial_vtx].position;
      glm::vec3 maxpos = vertices[initial_vtx].position;
      for (int i = initial_vtx; i < vertices.size(); i++) {
        minpos = min(minpos, vertices[i].position);
        maxpos = max(maxpos, vertices[i].position);
      }
    }
  }
  mesh_buffers_ = resource_manager_.upload_mesh(indices_, vertices_);

  // hierachy
  for (fastgltf::Node& node : gltf.nodes) {
    scene_object new_node;

    if (node.meshIndex.has_value()) {
      new_node = find_scene_object_by_mesh(mesh_offset + *node.meshIndex).get();
      new_node.name = node.name;
    } else {
      entity entity = create_entity();
      new_node = find_scene_object_by_entity(entity).get();
      new_node.name = node.name;
    }


    std::visit(fastgltf::visitor{
                   [&](fastgltf::Node::TransformMatrix matrix) {
                     glm::mat4 transformation;
                     memcpy(&transformation, matrix.data(), sizeof(matrix));
                     glm::vec3 scale;
                     glm::quat rotation;
                     glm::vec3 translation;
                     glm::vec3 skew;
                     glm::vec4 perspective;
                     decompose(transformation, scale, rotation, translation, skew, perspective);
                     set_transform_component(new_node.entity, translation, rotation, scale);
                   },
                   [&](fastgltf::Node::TRS transform) {
                     glm::vec3 translation(transform.translation[0], transform.translation[1],
                                           transform.translation[2]);
                     glm::quat rotation(transform.rotation[3], transform.rotation[0],
                                        transform.rotation[1], transform.rotation[2]);
                     glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);

                     set_transform_component(new_node.entity, translation, rotation, scale);
                   }},
               node.transform);
  }

  for (int i = 0; i < gltf.nodes.size(); i++) {
    fastgltf::Node& node = gltf.nodes[i];
    auto& scene_object = find_scene_object_by_entity(node_offset + i).get();

    for (auto& c : node.children) {
      auto child = find_scene_object_by_entity(node_offset + c).get();
      scene_object.children.push_back(child.entity);
      child.parent = scene_object.entity;
    }
  }

  // find the top nodes, with no parents
  for (auto& node : entity_hierarchy_) {
    if (node.second.parent == invalid_entity) {
      root_.children.push_back(node.first);
      node.second.parent = root_.entity;
    }
  }
}

void vk_scene_manager::load_scene_from_gltf() {

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
               add_mesh_component(entity, vertices, indices,
            "mesh_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k));
        set_transform_component(entity, glm::vec3(i - 5.f, j - 5.f, k -5.f),
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

          deletion_service_.push_function([this, default_material]() {
            resource_manager_.destroy_image(default_material.color_image);
          });
          deletion_service_.push_function([this, default_material]() {
            resource_manager_.destroy_image(default_material.metallic_roughness_image);
          });
          deletion_service_.push_function([this, default_material]() {
            resource_manager_.destroy_image(default_material.normal_image);
          });
          deletion_service_.push_function([this, default_material]() {
            resource_manager_.destroy_image(default_material.emissive_image);
          });
          deletion_service_.push_function([this, default_material]() {
            resource_manager_.destroy_image(default_material.occlusion_image);
          });
          deletion_service_.push(default_material.default_sampler_nearest);
          deletion_service_.push(default_material.default_sampler_linear);
          deletion_service_.push_function([this, materialDataBuffer]() {
            resource_manager_.destroy_buffer(materialDataBuffer);
          });
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
  deletion_service_.push_function([this]() { resource_manager_.destroy_buffer(mesh_buffers_.indexBuffer); });
  deletion_service_.push_function([this]() { resource_manager_.destroy_buffer(mesh_buffers_.vertexBuffer); });

}

entity vk_scene_manager::create_entity() {
  const entity new_entity = {next_entity_id_++};
  entities_.push_back(new_entity);

  const scene_object object = {.entity = new_entity};
  entity_hierarchy_[new_entity] = object;

  set_transform_component(new_entity, glm::vec3(0));

  return new_entity;
}

void vk_scene_manager::set_transform_component(entity entity, const glm::vec3& position,
                                               const glm::quat& rotation, const glm::vec3& scale) {

  auto& scene_object = find_scene_object_by_entity(entity).get();

  if (scene_object.transform == no_component) {
    const transform_component transform{position, rotation, scale};
    transforms_.push_back(transform);
    scene_object.transform = transforms_.size() - 1;
  } else {
    auto& transform = transforms_[scene_object.transform];
    transform = {position, rotation, scale};
  }
}

void vk_scene_manager::add_mesh_component(const entity entity, std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices, const std::string& name) {

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
  mesh_map_[name] = meshes_.size() - 1;
  scene_object& object = entity_hierarchy_[entity];
  object.mesh = meshes_.size() - 1;
}

void vk_scene_manager::add_camera_component(entity entity, const CameraComponent& camera) {
  cameras_.push_back(camera);
  scene_object& object = entity_hierarchy_[entity];
  object.camera = cameras_.size() - 1;
}

void vk_scene_manager::add_light_component(entity entity, const LightComponent& light) {
  lights_.push_back(light);
  scene_object& object = entity_hierarchy_[entity];
  object.camera = lights_.size() - 1;
}

void vk_scene_manager::add_material(MaterialPass pass_type,
                                    const gltf_metallic_roughness::MaterialResources& resources,
                                    const std::string& name) {
  materials_.emplace_back(material_component{
      .name = name,
      .data = gltf_material_.write_material(gpu_.device, pass_type, resources, descriptorPool)});
  const std::string key = name.empty() ? "material_" + std::to_string(materials_.size()) : name;
  material_map_[key] = materials_.size() - 1;
}

void vk_scene_manager::add_material_component(const entity entity, const std::string& name) {
  scene_object& object = entity_hierarchy_[entity];
  size_t material_index = material_map_[name];
  object.material = material_index;
}

const std::vector<entity>& vk_scene_manager::get_children(entity entity) const {
  return entity_hierarchy_.at(entity).children;
}

std::reference_wrapper<scene_object> vk_scene_manager::find_scene_object_by_mesh(size_t mesh_id) {
  auto it = std::ranges::find_if(
      entity_hierarchy_, [mesh_id](const auto& pair) { return pair.second.mesh == mesh_id; });

  if (it != entity_hierarchy_.end()) {
    return it->second;
  }
  fmt::print("could not find scene object for mesh {}", mesh_id);
}

std::reference_wrapper<scene_object> vk_scene_manager::
find_scene_object_by_entity(entity entity) {
  if (entity == invalid_entity) {
    fmt::print("entity is not valid: {}", entity);
  }
  auto it = entity_hierarchy_.find(entity);
  if (it != entity_hierarchy_.end()) {
    return it->second;
  }
  fmt::print("could not find scene object for entity {}", entity);
}

std::reference_wrapper<scene_object> vk_scene_manager::find_scene_object_by_name(
    std::string name) {
  auto it = std::ranges::find_if(entity_hierarchy_,
                                 [&name](const auto& pair) { return pair.second.name == name; });

  if (it != entity_hierarchy_.end()) {
    return it->second;
  }
  fmt::print("could not find scene object for name {}", name);
}

void vk_scene_manager::update_scene(draw_context& draw_context) {

  // Start the traversal from the root nodes
  for (const auto& rootNodeEntity : root_.children) {
    traverse_scene(rootNodeEntity, draw_context);
  }
}


void vk_scene_manager::traverse_scene(entity nodeEntity, draw_context& draw_context) {
  const auto& object = entity_hierarchy_.at(nodeEntity);
  if (object.entity != invalid_entity && object.mesh != no_component
      && object.material != no_component) {
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

  // Process child nodes recursively
  for (const auto& childEntity : object.children) {
    traverse_scene(childEntity, draw_context);
  }
};

void vk_scene_manager::init_default_data() {

  
  uint32_t white = 0xFFFFFFFF;                            // White color for color and occlusion
  uint32_t default_metallic_roughness = 0xFF00FF00;     // Green color representing metallic-roughness
  uint32_t flat_normal = 0xFFFF8080;                    // Flat normal
  uint32_t black = 0xFF000000;                          // Black color for emissive

  default_material_.color_image = resource_manager_.create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material_.metallic_roughness_image
      = resource_manager_.create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material_.normal_image
      = resource_manager_.create_image((void*)&flat_normal, VkExtent3D{1, 1, 1},
                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material_.emissive_image = resource_manager_.create_image(
      (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material_.occlusion_image = resource_manager_.create_image(
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
  default_material_.error_checkerboard_image = resource_manager_.create_image(
      pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;

  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material_.default_sampler_nearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material_.default_sampler_linear);

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
  material_resources.colorImage = default_material_.color_image;
  material_resources.colorSampler = default_material_.default_sampler_linear;
  material_resources.metalRoughImage = default_material_.metallic_roughness_image;
  material_resources.metalRoughSampler = default_material_.default_sampler_linear;
  material_resources.normalImage = default_material_.normal_image;
  material_resources.normalSampler = default_material_.default_sampler_linear;
  material_resources.emissiveImage = default_material_.emissive_image;
  material_resources.emissiveSampler = default_material_.default_sampler_linear;
  material_resources.occlusionImage = default_material_.occlusion_image;
  material_resources.occlusionSampler = default_material_.default_sampler_nearest;

  // set the uniform buffer for the material data
  material_resources.dataBuffer = materialDataBuffer.buffer;
  material_resources.dataBufferOffset
      = data_index * sizeof(gltf_metallic_roughness::MaterialConstants);

  // build material
  add_material(pass_type, material_resources, default_material_name_);

  deletion_service_.push_function([this]() {
    resource_manager_.destroy_image(default_material_.color_image);
  }); 
  deletion_service_.push_function([this]() {
    resource_manager_.destroy_image(default_material_.metallic_roughness_image);
  }); 
  deletion_service_.push_function([this]() {
    resource_manager_.destroy_image(default_material_.normal_image);
  }); 
  deletion_service_.push_function([this]() {
    resource_manager_.destroy_image(default_material_.emissive_image);
  }); 
  deletion_service_.push_function([this]() {
    resource_manager_.destroy_image(default_material_.occlusion_image);
  }); 
  deletion_service_.push_function([this]() {
    resource_manager_.destroy_image(default_material_.error_checkerboard_image);
  }); 
  deletion_service_.push(default_material_.default_sampler_nearest);
  deletion_service_.push(default_material_.default_sampler_linear);
  deletion_service_.push_function([this, materialDataBuffer]() {
       resource_manager_.destroy_buffer(materialDataBuffer);
  });
  deletion_service_.push_function([this]() { descriptorPool.destroy_pools(gpu_.device); });
  deletion_service_.push(materials_.back().data.pipeline->layout);
}