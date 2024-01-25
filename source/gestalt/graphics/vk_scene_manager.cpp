
#include <vk_scene_manager.h>

#include "vk_engine.h"
#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include <glm/gtx/matrix_decompose.hpp>

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
  //load_scene_from_gltf(R"(..\..\assets\Models\MetalRoughSpheres\glTF-Binary\MetalRoughSpheres.glb)");
  //load_scene_from_gltf(R"(..\..\assets\sponza_pestana.glb)");
  load_scene_from_gltf(R"(..\..\assets\structure.glb)");
}

void vk_scene_manager::cleanup() {
  for (auto img : images_) {
    resource_manager_.destroy_image(img);
  }

  deletion_service_.flush();
}

void vk_scene_manager::load_scene_from_gltf(const std::string& file_path) {

  fmt::print("Loading GLTF: {}\n", file_path);

  fastgltf::Parser parser{};

  constexpr auto gltf_options
      = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::None;
  // fastgltf::Options::LoadExternalImages;

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

    VkSampler new_sampler;
    vkCreateSampler(gpu_.device, &sampl, nullptr, &new_sampler);

    samplers_.push_back(new_sampler);
    deletion_service_.push(new_sampler);
  }

  for (fastgltf::Image& image : gltf.images) {
    std::optional<AllocatedImage> img = resource_manager_.load_image(gltf, image);

    if (img.has_value()) {
      images_.push_back(img.value());
    } else {
      fmt::print("gltf failed to load texture {}\n", image.name);
      images_.push_back(default_material_.error_checkerboard_image);
    }
  }

  // load buffer and materials
  size_t material_offset = materials_.size();
  material_data_buffer_ = resource_manager_.create_buffer(
      sizeof(gltf_metallic_roughness::MaterialConstants) * gltf.materials.size(),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  gltf_metallic_roughness::MaterialConstants* sceneMaterialConstants
      = (gltf_metallic_roughness::MaterialConstants*)material_data_buffer_.info.pMappedData;
  //< l

  for (fastgltf::Material& mat : gltf.materials) {


    pbr_config pbr_config{};
    gltf_metallic_roughness::MaterialConstants constants;
    constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
    constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
    constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
    constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

    constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
    constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
    // write material parameters to buffer
    sceneMaterialConstants[data_index] = constants;

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
      pbr_config.albedo_factor
          = glm::vec4(mat.pbrData.baseColorFactor.at(0), mat.pbrData.baseColorFactor.at(1),
                      mat.pbrData.baseColorFactor.at(2), mat.pbrData.baseColorFactor.at(3));
      pbr_config.use_albedo_tex = true;
      pbr_config.albedo_tex = img;
    }
    if (mat.pbrData.metallicRoughnessTexture) {
      size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex]
                       .imageIndex.value();
      size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex]
                           .samplerIndex.value();

      materialResources.metalRoughImage = images_[img];
      materialResources.metalRoughSampler = samplers_[sampler];
      pbr_config.metal_rough_tex = true;
      pbr_config.metal_rough_factor
          = glm::vec2(mat.pbrData.metallicFactor, mat.pbrData.roughnessFactor);
      pbr_config.metal_rough_tex = img;

    }
    if (mat.normalTexture.has_value()) {
      size_t img = gltf.textures[mat.normalTexture.value().textureIndex]
                       .imageIndex.value();
      size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex]
                           .samplerIndex.value();

      materialResources.normalImage = images_[img];
      materialResources.normalSampler = samplers_[sampler];
      pbr_config.use_normal_tex = true;
      pbr_config.normal_scale = 1.f; //TODO
      pbr_config.normal_tex = img;
    }
    if (mat.emissiveTexture.has_value()) {
      size_t img = gltf.textures[mat.emissiveTexture.value().textureIndex]
                       .imageIndex.value();
      size_t sampler = gltf.textures[mat.emissiveTexture.value().textureIndex]
                           .samplerIndex.value();

      materialResources.emissiveImage = images_[img];
      materialResources.emissiveSampler = samplers_[sampler];
      pbr_config.use_emissive_tex = true;
      pbr_config.emissive_factor
          = glm::vec3(mat.emissiveFactor.at(0), mat.emissiveFactor.at(1), mat.emissiveFactor.at(2));
      pbr_config.emissive_tex = img;
      pbr_config.emissive_factor; //TODO

    }
    if (mat.occlusionTexture.has_value()) {
      size_t img = gltf.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
      size_t sampler
          = gltf.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();

      materialResources.occlusionImage = images_[img];
      materialResources.occlusionSampler = samplers_[sampler];
      pbr_config.use_occlusion_tex = true;
      pbr_config.occlusion_tex = img;
      pbr_config.occlusion_strength = 1.f; //TODO
    }
    // build material
    create_material(pass_type, materialResources, pbr_config, std::string(mat.name));
    data_index++;
  }


  // load meshes
  std::vector<size_t> surfaces;

  for (fastgltf::Mesh& mesh : gltf.meshes) {
    surfaces.clear();

    for (auto&& primitive : mesh.primitives) {

      size_t initial_index = vertices_.size();
      std::vector<uint32_t> indices;
      std::vector<Vertex> vertices;

      // load indexes
      {
        fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
          indices.push_back(idx + initial_index);
        });
      }

      // load vertex positions
      {
        fastgltf::Accessor& posAccessor
            = gltf.accessors[primitive.findAttribute("POSITION")->second];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                      [&](glm::vec3 v, size_t index) {
                                                        Vertex newvtx;
                                                        newvtx.position = v;
                                                        newvtx.normal = {1, 0, 0};
                                                        newvtx.color = glm::vec4{1.f};
                                                        newvtx.uv_x = 0;
                                                        newvtx.uv_y = 0;
                                                        vertices[index] = newvtx;
                                                      });
      }

      // load vertex normals
      auto normals = primitive.findAttribute("NORMAL");
      if (normals != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, gltf.accessors[(*normals).second],
            [&](glm::vec3 v, size_t index) { vertices[index].normal = v; });
      }

      // load UVs
      auto uv = primitive.findAttribute("TEXCOORD_0");
      if (uv != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                                                      [&](glm::vec2 v, size_t index) {
                                                        vertices[index].uv_x = v.x;
                                                        vertices[index].uv_y = v.y;
                                                      });
      }

      // load vertex colors
      auto colors = primitive.findAttribute("COLOR_0");
      if (colors != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            gltf, gltf.accessors[(*colors).second],
            [&](glm::vec4 v, size_t index) { vertices[index].color = v; });
      }

      size_t surface_index = create_surface(vertices, indices);
      if (primitive.materialIndex.has_value()) {
        add_material_component(surface_index, material_offset + primitive.materialIndex.value());

      } else {
        add_material_component(surface_index, default_material);
      }
      surfaces.push_back(surface_index);
    }
    create_mesh(surfaces, std::string(mesh.name));
  }
  mesh_buffers_ = resource_manager_.upload_mesh(indices_, vertices_);

  // hierachy
  for (fastgltf::Node& node : gltf.nodes) {
    entity_component& new_node = create_entity();

    if (node.meshIndex.has_value()) {
      add_mesh_component(new_node.entity, *node.meshIndex);
    }

    if (std::holds_alternative<fastgltf::Node::TRS>(node.transform)) {
      const auto& trs = std::get<fastgltf::Node::TRS>(node.transform);
      glm::vec3 translation(trs.translation[0], trs.translation[1], trs.translation[2]);
      glm::quat rotation(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
      glm::vec3 scale(trs.scale[0], trs.scale[1], trs.scale[2]);

      set_transform_component(new_node.entity, translation, rotation, scale);
    }
  }

  assert(gltf.nodes.size() == scene_graph_.size());

  for (int i = 0; i < gltf.nodes.size(); i++) {
    fastgltf::Node& node = gltf.nodes[i];
    auto& scene_object = get_scene_object_by_entity(i).value().get();

    for (auto& c : node.children) {
      auto child = get_scene_object_by_entity(c).value().get();
      scene_object.children.push_back(child.entity);
      child.parent = scene_object.entity;
    }
  }

  // find the top nodes, with no parents
  for (auto& node : scene_graph_) {
    if (node.parent == invalid_entity) {
      root_.children.push_back(node.entity);
    }
  }
}

entity_component& vk_scene_manager::create_entity() {
  const entity new_entity = {next_entity_id_++};

  const entity_component object = {
    .name = "entity_" + std::to_string(new_entity),
    .entity = new_entity,
  };
  scene_graph_.push_back(object);

  set_transform_component(new_entity, glm::vec3(0));

  return scene_graph_.at(new_entity);
}

void vk_scene_manager::set_transform_component(entity entity, const glm::vec3& position,
                                               const glm::quat& rotation, const glm::vec3& scale) {
  assert(entity != invalid_entity);
  const auto& transform = transforms_.emplace_back(position, rotation, scale);
  matrices_.push_back(transform.get_model_matrix());
  transform.is_dirty = false;

  auto& scene_object = get_scene_object_by_entity(entity).value().get();
  scene_object.transform = transforms_.size() - 1;
}

size_t vk_scene_manager::create_surface(std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices) {
  assert(!vertices.empty() && !indices.empty());

  glm::vec3 minpos = vertices.empty() ? glm::vec3(0) : vertices[0].position;
  glm::vec3 maxpos = minpos;
  for (const auto& vertice : vertices) {
    minpos = min(minpos, vertice.position);
    maxpos = max(maxpos, vertice.position);
  }
  glm::vec3 extents = (maxpos - minpos) / 2.f;
  Bounds bounds{
      .origin = (maxpos + minpos) / 2.f,
      .extents = extents,
      .sphereRadius = length(extents),
  };

  const mesh_surface surface{
      .vertex_count = static_cast<uint32_t>(vertices.size()),
      .index_count = static_cast<uint32_t>(indices.size()),
      .first_index = static_cast<uint32_t>(indices_.size()),
      .vertex_offset = static_cast<uint32_t>(vertices_.size()),
      .bounds = bounds,
  };

  vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
  uint32_t highest_index = *std::max_element(indices.begin(), indices.end());
  assert(highest_index <= vertices_.size());
  indices_.insert(indices_.end(), indices.begin(), indices.end());

  surfaces_.push_back(surface);

  return surfaces_.size() - 1;
}

size_t vk_scene_manager::create_mesh(std::vector<size_t> surfaces, const std::string& name) {

  meshes_.emplace_back(mesh_component{surfaces});
  const std::string key = name.empty() ? "mesh_" + std::to_string(meshes_.size()) : name;

  return meshes_.size() - 1;
}

void vk_scene_manager::add_mesh_component(const entity entity, size_t mesh_index) {
  assert(entity != invalid_entity);

  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.mesh = mesh_index;
}

void vk_scene_manager::add_camera_component(entity entity, const CameraComponent& camera) {
  cameras_.push_back(camera);
  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.camera = cameras_.size() - 1;
}

void vk_scene_manager::add_light_component(entity entity, const LightComponent& light) {
  lights_.push_back(light);
  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.camera = lights_.size() - 1;
}

size_t vk_scene_manager::create_material(
    MaterialPass pass_type, const gltf_metallic_roughness::MaterialResources& resources,
    const pbr_config& config, const std::string& name) {

  const std::string key = name.empty() ? "material_" + std::to_string(materials_.size()) : name;

  materials_.emplace_back(material_component{
      .name = key,
      .data = gltf_material_.write_material(gpu_.device, pass_type, resources, descriptorPool),
      .config = config
  });

  return materials_.size() - 1;
}

void vk_scene_manager::add_material_component(const size_t surface, const size_t material) {
  assert(material != no_component);
  assert(material <= materials_.size());
  assert(surface <= surfaces_.size());
  assert(surfaces_[surface].material == default_material);
  surfaces_[surface].material = material;
}

const std::vector<entity>& vk_scene_manager::get_children(entity entity) {
  assert(entity != invalid_entity);
  return get_scene_object_by_entity(entity).value().get().children;
}

 std::optional<std::reference_wrapper<entity_component>> vk_scene_manager::get_scene_object_by_entity(entity entity) {
  assert(entity != invalid_entity);

  if (scene_graph_.size() >= entity) {
    return scene_graph_.at(entity);
  }

  fmt::print("could not find scene object for entity {}", entity);
  return std::nullopt;
 }


void vk_scene_manager::update_scene(draw_context& draw_context) {

  glm::mat4 identity = glm::mat4(1.0f);
  for (const auto& root_node_entity : root_.children) {
    traverse_scene(root_node_entity, identity, draw_context);
  }
}

void vk_scene_manager::traverse_scene(const entity entity, const glm::mat4& parent_transform,
                                      draw_context& draw_context) {
  assert(entity != invalid_entity);
  const auto& object = get_scene_object_by_entity(entity).value().get();
  const auto& transform = transforms_.at(object.transform);
  if (transform.is_dirty) {
    matrices_.at(object.transform) = transform.get_model_matrix();
    transform.is_dirty = false;
  }
  const glm::mat4 world_transform = parent_transform * matrices_.at(object.transform);

  if (object.is_valid() && object.has_mesh()) {
    const auto& mesh = meshes_.at(object.mesh);

    for (const auto surface_index : mesh.surfaces) {
      const auto& surface = surfaces_.at(surface_index);
      auto& material = materials_.at(surface.material);

      render_object def;
      def.index_count = surface.index_count;
      def.first_index = surface.first_index;
      def.index_buffer = mesh_buffers_.indexBuffer.buffer;
      def.material = &material.data;
      def.bounds = surface.bounds;
      def.transform = world_transform;
      def.vertex_buffer_address = mesh_buffers_.vertexBufferAddress;

      if (material.data.passType == MaterialPass::MainColor) {
        draw_context.opaque_surfaces.push_back(def);
      } else {
        draw_context.transparent_surfaces.push_back(def);
      }
    }
  }

  // Process child nodes recursively, passing the current world transform
  for (const auto& childEntity : object.children) {
    traverse_scene(childEntity, world_transform, draw_context);
  }
}

void vk_scene_manager::init_default_data() {

  
  uint32_t white = 0xFFFFFFFF;                          // White color for color and occlusion
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
  pbr_config config{};
  // build material
  create_material(pass_type, material_resources, config, "default_material");

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