#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <meshoptimizer.h>

#include "scene_manager.h"

struct Vertex {
   glm::vec3 position;
  glm::vec3 normal;
   glm::vec4 tangent;
  glm::vec2 uv;
};


void asset_loader::init(const vk_gpu& gpu,
                        const std::shared_ptr<resource_manager>& resource_manager,
                        const std::shared_ptr<component_archetype_factory>& component_factory,
                        const std::shared_ptr<Repository>& repository) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;
  repository_ = repository;
  component_factory_ = component_factory;
}

void asset_loader::import_lights(const fastgltf::Asset& gltf) {
  fmt::print("importing lights\n");
  for (auto& light : gltf.lights) {
    if (light.type == fastgltf::LightType::Directional) {
      component_factory_->create_directional_light(
          glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)), light.intensity,
          glm::vec3(0.f));

    } else if (light.type == fastgltf::LightType::Point) {
      component_factory_->create_point_light(
          glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)), light.intensity,
          glm::vec3(0.f));

    } else if (light.type == fastgltf::LightType::Spot) {
      // TODO;
    }
  }
}

std::vector<fastgltf::Node> asset_loader::load_scene_from_gltf(const std::string& file_path) {
  fmt::print("Loading GLTF: {}\n", file_path);

  fastgltf::Asset gltf = parse_gltf(file_path).value();

  size_t image_offset = repository_->textures.size();
  size_t sampler_offset = repository_->samplers.size();
  size_t material_offset = repository_->materials.size();

  import_samplers(gltf);
  import_textures(gltf);

  import_materials(gltf, sampler_offset, image_offset);

  import_meshes(gltf, material_offset);

  import_lights(gltf);

  return gltf.nodes;
}

VkFilter asset_loader::extract_filter(fastgltf::Filter filter) {
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

VkSamplerMipmapMode asset_loader::extract_mipmap_mode(fastgltf::Filter filter) {
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

std::optional<fastgltf::Asset> asset_loader::parse_gltf(const std::filesystem::path& file_path) {
  static constexpr auto gltf_extensions
      = fastgltf::Extensions::KHR_lights_punctual
        | fastgltf::Extensions::KHR_materials_emissive_strength
        | fastgltf::Extensions::KHR_materials_clearcoat | fastgltf::Extensions::KHR_materials_ior
        | fastgltf::Extensions::KHR_materials_sheen | fastgltf::Extensions::None;

  fastgltf::Parser parser{gltf_extensions};
  constexpr auto gltf_options
      = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::LoadExternalImages
        | fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::None;

  fastgltf::GltfDataBuffer data;
  if (!data.loadFromFile(file_path)) {
    fmt::print("Failed to load file data from path: {}\n", file_path.string());
    return std::nullopt; 
  }

  const std::filesystem::path& path = file_path;

  auto load_result = parser.loadGltf(&data, path.parent_path(), gltf_options);
  if (load_result) {
    return std::move(load_result.get());
  }
  fmt::print("Failed to load glTF: {}\n", to_underlying(load_result.error()));

  return std::nullopt; 
}

void asset_loader::import_samplers(fastgltf::Asset& gltf) {
  fmt::print("importing samplers\n");
  for (fastgltf::Sampler& sampler : gltf.samplers) {
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.minLod = 0;

    sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
    sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    VkSampler new_sampler;
    vkCreateSampler(gpu_.device, &sampl, nullptr, &new_sampler);

    fmt::print("created sampler {}, sampler_id {}\n", sampler.name, repository_->samplers.size());
    repository_->samplers.add(new_sampler);
  }
}

std::optional<AllocatedImage> asset_loader::load_image(fastgltf::Asset& asset,
                                                       fastgltf::Image& image) const {
  AllocatedImage newImage = {};
  int width = 0, height = 0, nr_channels = 0;

  const std::function<void(unsigned char*)> create_image = [&](unsigned char* data) {
    if (data) {
      newImage = resource_manager_->create_image(
          data, VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

      stbi_image_free(data);
    }
  };

  const std::function<void(fastgltf::sources::URI&)> create_image_from_file
      = [&](fastgltf::sources::URI& file_path) {
          fmt::print("Loading image from file: {}\n", file_path.uri.string());
          assert(file_path.fileByteOffset == 0);  // We don't support offsets with stbi.
          assert(file_path.uri.isLocalPath());    // We're only capable of loading local files.

          const std::string filename(file_path.uri.path().begin(), file_path.uri.path().end());
          unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nr_channels, 4);
          create_image(data);
        };

  const std::function<void(fastgltf::sources::Array&)> create_image_from_vector
      = [&](fastgltf::sources::Array& vector) {
          unsigned char* data
              = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                                      &width, &height, &nr_channels, 4);
          create_image(data);
        };

  const std::function<void(fastgltf::sources::BufferView&)> create_image_from_buffer_view
      = [&](fastgltf::sources::BufferView& view) {
          const auto& buffer_view = asset.bufferViews[view.bufferViewIndex];
          auto& buffer = asset.buffers[buffer_view.bufferIndex];

          std::visit(fastgltf::visitor{[](auto& arg) {},
                                       [&](fastgltf::sources::Array& vector) {
                                         unsigned char* data = stbi_load_from_memory(
                                             vector.bytes.data() + buffer_view.byteOffset,
                                             static_cast<int>(buffer_view.byteLength), &width,
                                             &height, &nr_channels, 4);
                                         create_image(data);
                                       }},
                     buffer.data);
        };

  std::visit(
      fastgltf::visitor{
          [](auto& arg) {},
          [&](fastgltf::sources::URI& file_path) { create_image_from_file(file_path); },
          [&](fastgltf::sources::Array& vector) { create_image_from_vector(vector); },
          [&](fastgltf::sources::BufferView& view) { create_image_from_buffer_view(view); },
      },
      image.data);

  if (newImage.image == VK_NULL_HANDLE) {
    return {};
  }
  return newImage;
}

void asset_loader::import_textures(fastgltf::Asset& gltf) const {
  fmt::print("importing textures\n");
  for (fastgltf::Image& image : gltf.images) {
    std::optional<AllocatedImage> img = load_image(gltf, image);

    if (img.has_value()) {
      size_t image_id = repository_->textures.add(img.value());
      fmt::print("loaded texture {}, image_id {}\n", image.name, image_id);
    } else {
      fmt::print("gltf failed to load texture {}\n", image.name);
      repository_->textures.add(
          repository_->default_material_.error_checkerboard_image);
    }
  }
}

size_t asset_loader::create_material(pbr_material& config,
                                      const std::string& name) const {
  const size_t material_id = repository_->materials.size();
  fmt::print("creating material {}, mat_id {}\n", name, material_id);

  const std::string key = name.empty() ? "material_" + std::to_string(material_id) : name;
  resource_manager_->write_material(config, material_id);

  repository_->materials.add(material{.name = key, .config = config});

  return material_id;
}

std::tuple<AllocatedImage, VkSampler> asset_loader::get_textures(
    const fastgltf::Asset& gltf, const size_t& texture_index, const size_t& image_offset,
    const size_t& sampler_offset) const {
  const size_t image_index = gltf.textures[texture_index].imageIndex.value();
  const size_t sampler_index = gltf.textures[texture_index].samplerIndex.value();

  AllocatedImage image = repository_->textures.get(image_index + image_offset);
  VkSampler sampler = repository_->samplers.get(sampler_index + sampler_offset);
  return std::make_tuple(image, sampler);
}

void asset_loader::import_albedo(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                 const size_t& image_offset, const fastgltf::Material& mat,
                                 pbr_material& pbr_config) const {
  if (mat.pbrData.baseColorTexture.has_value()) {
    pbr_config.use_albedo_tex = true;
    pbr_config.constants.albedo_tex_index = 0;
    auto [image, sampler] = get_textures(gltf, mat.pbrData.baseColorTexture.value().textureIndex,
                                         image_offset, sampler_offset);

    pbr_config.resources.albedo_image = image;
    pbr_config.resources.albedo_sampler = sampler;
  } else {
    pbr_config.constants.albedo_factor
        = glm::vec4(mat.pbrData.baseColorFactor.at(0), mat.pbrData.baseColorFactor.at(1),
                    mat.pbrData.baseColorFactor.at(2), mat.pbrData.baseColorFactor.at(3));
  }
}

void asset_loader::import_metallic_roughness(
    const fastgltf::Asset& gltf, const size_t& sampler_offset, const size_t& image_offset,
    const fastgltf::Material& mat, pbr_material& pbr_config) const {
  if (mat.pbrData.metallicRoughnessTexture.has_value()) {
    pbr_config.use_metal_rough_tex = true;
    pbr_config.constants.metal_rough_tex_index = 0;
    auto [image, sampler] = get_textures(gltf, mat.pbrData.metallicRoughnessTexture.value().textureIndex,
                                         image_offset, sampler_offset);

    pbr_config.resources.metal_rough_image = image;
    assert(image.image
           != repository_->default_material_.error_checkerboard_image.image);
  } else {
    pbr_config.constants.metal_rough_factor
        = glm::vec2(mat.pbrData.roughnessFactor, mat.pbrData.metallicFactor); // r - occlusion, g - roughness, b - metallic
  }
}

void asset_loader::import_normal(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                 const size_t& image_offset, const fastgltf::Material& mat,
                                 pbr_material& pbr_config) const {
  if (mat.normalTexture.has_value()) {
    pbr_config.use_normal_tex = true;
    pbr_config.constants.normal_tex_index = 0;
    auto [image, sampler]
        = get_textures(gltf, mat.normalTexture.value().textureIndex, image_offset, sampler_offset);

    pbr_config.resources.normal_image = image;
    pbr_config.resources.normal_sampler = sampler;
    //pbr_config.normal_scale = 1.f;  // TODO
  }
}

void asset_loader::import_emissive(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                   const size_t& image_offset, const fastgltf::Material& mat,
                                   pbr_material& pbr_config) const {
  if (mat.emissiveTexture.has_value()) {
    pbr_config.use_emissive_tex = true;
    pbr_config.constants.emissive_tex_index = 0;
    auto [image, sampler] = get_textures(gltf, mat.emissiveTexture.value().textureIndex,
                                         image_offset, sampler_offset);

    pbr_config.resources.emissive_image = image;
    pbr_config.resources.emissive_sampler = sampler;
  } else {
    glm::vec3 emissiveFactor
        = glm::vec3(mat.emissiveFactor.at(0), mat.emissiveFactor.at(1), mat.emissiveFactor.at(2));
    if (length(emissiveFactor) > 0) {
      emissiveFactor *= mat.emissiveStrength;
      pbr_config.constants.emissiveFactor = emissiveFactor;
    }
  }
}

void asset_loader::import_occlusion(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                    const size_t& image_offset, const fastgltf::Material& mat,
                                    pbr_material& pbr_config) const {
  if (mat.occlusionTexture.has_value()) {
    pbr_config.use_occlusion_tex = true;
    pbr_config.constants.occlusion_tex_index = 0;
    auto [image, sampler] = get_textures(gltf, mat.occlusionTexture.value().textureIndex,
                                         image_offset, sampler_offset);

    pbr_config.resources.occlusion_image = image;
    pbr_config.resources.occlusion_sampler = sampler;
    pbr_config.constants.occlusionStrength = 1.f;
  }
}

void asset_loader::import_material(fastgltf::Asset& gltf, size_t& sampler_offset, size_t& image_offset, fastgltf::Material& mat) const {
  pbr_material pbr_config{};

  if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
    pbr_config.transparent = true;
  } else if (mat.alphaMode == fastgltf::AlphaMode::Mask) {
    pbr_config.constants.alpha_cutoff = mat.alphaCutoff;
  }

  auto& default_material = repository_->default_material_;

  pbr_config.resources
      = {.albedo_image = default_material.color_image,
         .albedo_sampler = default_material.linearSampler,
         .metal_rough_image = default_material.metallic_roughness_image,
         .metal_rough_sampler = default_material.linearSampler,
         .normal_image = default_material.normal_image,
         .normal_sampler = default_material.linearSampler,
         .emissive_image = default_material.emissive_image,
         .emissive_sampler = default_material.linearSampler,
         .occlusion_image = default_material.occlusion_image,
         .occlusion_sampler = default_material.nearestSampler};

  // grab textures from gltf file
  import_albedo(gltf, sampler_offset, image_offset, mat, pbr_config);
  import_metallic_roughness(gltf, sampler_offset, image_offset, mat, pbr_config);
  import_normal(gltf, sampler_offset, image_offset, mat, pbr_config);
  import_emissive(gltf, sampler_offset, image_offset, mat, pbr_config);
  import_occlusion(gltf, sampler_offset, image_offset, mat, pbr_config);

  // build material
  create_material(pbr_config, std::string(mat.name));
}

void asset_loader::import_materials(fastgltf::Asset& gltf, size_t& sampler_offset,
                                    size_t& image_offset) const {
  fmt::print("importing materials\n");
  for (fastgltf::Material& mat : gltf.materials) {
    if (mat.name == "arch.002") {
           mat.name = "material_" + std::to_string(repository_->materials.size());
    }
    import_material(gltf, sampler_offset, image_offset, mat);
  }
}

void asset_loader::optimize_mesh(std::vector<Vertex>& vertices,
                                 std::vector<uint32_t>& indices) {


  // Step 1: Generate a remap table for vertex optimization
  std::vector<unsigned int> remap(vertices.size());
  size_t vertex_count
      = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(),
                                    vertices.size(), sizeof(Vertex));

  // Step 2: Remap indices
  std::vector<uint32_t> remappedIndices(indices.size());
  meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), remap.data());

  // Step 3: Remap vertices
  std::vector<Vertex> remappedVertices(vertices.size());
  meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertex_count, sizeof(Vertex),
                            remap.data());

  // Step 4: Vertex cache optimization
  meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(),
                              remappedIndices.size(), vertex_count);

  // Step 5: Overdraw optimization (optional, depending on your needs)
  meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), reinterpret_cast<const float*>(remappedVertices.data()), vertex_count, sizeof(Vertex), 1.05f);

  // Step 6: Vertex fetch optimization
  meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(),
                              remappedIndices.size(), remappedVertices.data(), vertex_count,
                              sizeof(Vertex));

  // Replace the original vectors with the optimized ones
  vertices.swap(remappedVertices);
  indices.swap(remappedIndices);
}


void asset_loader::simplify_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {

  const size_t index_count = indices.size();
  const size_t vertex_count = vertices.size();

  float complexityThreshold = 0.5f;  // 50% of the original complexity
  size_t target_index_count = index_count * complexityThreshold;
  float target_error = 1e-2f;
  unsigned int options = 0;

  std::vector<uint32_t> lod_indices(index_count);
  float lod_error = 0.0f;

  lod_indices.resize(meshopt_simplify(lod_indices.data(), indices.data(), index_count,
                                      reinterpret_cast<const float*>(vertices.data()), vertex_count,
                                      sizeof(Vertex), target_index_count, target_error, options,
                                      &lod_error));
  indices.swap(lod_indices);
}


std::vector<meshlet> asset_loader::generate_meshlets(std::vector<GpuVertexPosition>& vertices,
                                                     std::vector<uint32_t>& indices) {
  const size_t max_vertices = 64;
  const size_t max_triangles = 124;  // Must be divisible by 4
  const float cone_weight = 0.0f;    // Set between 0 and 1 if using cone culling

  size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);
  std::vector<meshopt_Meshlet> meshopt_meshlets(max_meshlets);
  std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
  std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

  size_t meshlet_count = meshopt_buildMeshlets(
      meshopt_meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), indices.data(),
      indices.size(), reinterpret_cast<const float*>(vertices.data()), vertices.size(),
      sizeof(GpuVertexPosition), max_vertices, max_triangles, cone_weight);

  // Resize the arrays based on actual meshlet count and last meshlet's offsets + counts
  const auto& last = meshopt_meshlets[meshlet_count - 1];
  meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
  meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
  meshopt_meshlets.resize(meshlet_count);

  std::vector<meshlet> result_meshlets(meshlet_count);
  for (size_t i = 0; i < meshlet_count; ++i) {
    const auto& local_meshlet = meshopt_meshlets[i];
    auto& meshlet = result_meshlets[i];

    
    meshopt_Bounds meshlet_bounds = meshopt_computeMeshletBounds(
        meshlet_vertices.data() + local_meshlet.vertex_offset,
        meshlet_triangles.data() + local_meshlet.triangle_offset, local_meshlet.triangle_count,
        reinterpret_cast<float*>(vertices.data()), vertices.size(), sizeof(GpuVertexPosition));
        

    meshlet.data_offset = result_meshlets.size();
    meshlet.vertex_count = local_meshlet.vertex_count;
    meshlet.triangle_count = local_meshlet.triangle_count;
    
    meshlet.center
        = glm::vec3{meshlet_bounds.center[0], meshlet_bounds.center[1], meshlet_bounds.center[2]};
    meshlet.radius = meshlet_bounds.radius;

    meshlet.cone_axis[0] = meshlet_bounds.cone_axis_s8[0];
    meshlet.cone_axis[1] = meshlet_bounds.cone_axis_s8[1];
    meshlet.cone_axis[2] = meshlet_bounds.cone_axis_s8[2];

    meshlet.cone_cutoff = meshlet_bounds.cone_cutoff_s8;
    meshlet.mesh_index = repository_->meshes.size(); //TODO
  }

  return result_meshlets;
}

mesh_surface asset_loader::create_surface(std::vector<GpuVertexPosition>& vertex_positions,
                                          std::vector<GpuVertexData>& vertex_data,
                                          std::vector<uint32_t>& indices,
                                          std::vector<meshlet>&& meshlets) {
  assert(!vertex_positions.empty() && !indices.empty());
  assert(vertex_positions.size() == vertex_data.size());

  const size_t vertex_count = vertex_positions.size();
  const size_t index_count = indices.size();

  // Initialize local AABB with first vertex position
  AABB local_aabb{vertex_positions[0].position, vertex_positions[0].position};

  // Update local AABB for each vertex
  for (size_t i = 1; i < vertex_count; ++i) {
    const glm::vec3& position = vertex_positions[i].position;

    // Update min position
    local_aabb.min.x = std::min(local_aabb.min.x, position.x);
    local_aabb.min.y = std::min(local_aabb.min.y, position.y);
    local_aabb.min.z = std::min(local_aabb.min.z, position.z);

    // Update max position
    local_aabb.max.x = std::max(local_aabb.max.x, position.x);
    local_aabb.max.y = std::max(local_aabb.max.y, position.y);
    local_aabb.max.z = std::max(local_aabb.max.z, position.z);
  }

  mesh_surface surface{
      .meshlets = std::move(meshlets),
      .vertex_count = static_cast<uint32_t>(vertex_count),
      .index_count = static_cast<uint32_t>(index_count),
      .first_index = static_cast<uint32_t>(repository_->indices.size()),
      .vertex_offset = static_cast<uint32_t>(repository_->vertex_positions.size()),
      .local_bounds = local_aabb,
  };

  repository_->vertex_positions.add(vertex_positions);
  repository_->vertex_data.add(vertex_data);
  const uint32_t highest_index = *std::ranges::max_element(indices);
  assert(highest_index <= repository_->vertex_positions.size());
  repository_->indices.add(indices);

  return surface;
}

size_t asset_loader::create_mesh(std::vector<mesh_surface> surfaces, const std::string& name) const {
  size_t mesh_id = repository_->meshes.size();
  const std::string key = name.empty() ? "mesh_" + std::to_string(mesh_id) : name;
  AABB aabb;
  for (mesh_surface& surface : surfaces) {
    auto& [min, max, is_dirty] = surface.local_bounds;
    aabb.min.x = std::min(aabb.min.x, min.x);
    aabb.min.y = std::min(aabb.min.y, min.y);
    aabb.min.z = std::min(aabb.min.z, min.z);

    aabb.max.x = std::max(aabb.max.x, max.x);
    aabb.max.y = std::max(aabb.max.y, max.y);
    aabb.max.z = std::max(aabb.max.z, max.z);
  }
  mesh_id = repository_->meshes.add(mesh{key, std::move(surfaces), aabb});
  fmt::print("created mesh {}, mesh_id {}\n", key, mesh_id);
  return mesh_id;
}

void asset_loader::add_material_component(mesh_surface& surface, const size_t material) const {
  assert(material != no_component);
  assert(material <= repository_->materials.size());
  assert(surface.material == default_material);
  surface.material = material;
}

void asset_loader::import_meshes(fastgltf::Asset& gltf, const size_t material_offset) {
  std::vector<mesh_surface> surfaces;

  fmt::print("importing meshes\n");
  for (fastgltf::Mesh& mesh : gltf.meshes) {
    surfaces.clear();

    for (auto&& primitive : mesh.primitives) {
      size_t initial_index = repository_->vertex_positions.size();
      std::vector<uint32_t> indices;
      std::vector<Vertex> vertices;

      {
        fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
            gltf, indexaccessor, [&](std::uint32_t idx) { indices.push_back(idx); });
      }

      {
        fastgltf::Accessor& posAccessor
            = gltf.accessors[primitive.findAttribute("POSITION")->second];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                      [&](glm::vec3 v, size_t index) {
                                                            Vertex vtx;        
                                                            vtx.position = v;
                                                        vertices[index] = vtx;
                                                      });
      }

      auto normals = primitive.findAttribute("NORMAL");
      if (normals != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, gltf.accessors[(*normals).second],
            [&](glm::vec3 v, size_t index) { vertices[index].normal = v; });
      }

      auto tangents = primitive.findAttribute("TANGENT");
      if (tangents != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            gltf, gltf.accessors[(*tangents).second],
            [&](glm::vec4 v, size_t index) { vertices[index].tangent = v; });
      }

      auto uv = primitive.findAttribute("TEXCOORD_0");
      if (uv != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                                                      [&](glm::vec2 v, size_t index) {
                                                        vertices[index].uv = v;

                                                      });
      }

      optimize_mesh(vertices, indices);

      if (indices.size() > 8192) {
        simplify_mesh(vertices, indices);
      }

      std::vector<GpuVertexPosition> vertex_positions{vertices.size()};
      std::vector<GpuVertexData> vertex_data{vertices.size()};
      for (int i = 0; i < vertex_positions.size(); i++) {
        vertex_positions[i].position = vertices[i].position;
        
        vertex_data[i].normal[0] = static_cast<uint8_t>((vertices[i].normal.x + 1.0f) * 127.5f);
        vertex_data[i].normal[1] = static_cast<uint8_t>((vertices[i].normal.y + 1.0f) * 127.5f);
        vertex_data[i].normal[2] = static_cast<uint8_t>((vertices[i].normal.z + 1.0f) * 127.5f);
        vertex_data[i].normal[3] = 0;  // padding

        vertex_data[i].tangent[0] = static_cast<uint8_t>((vertices[i].tangent.x + 1.0f) * 127.5f);
        vertex_data[i].tangent[1] = static_cast<uint8_t>((vertices[i].tangent.y + 1.0f) * 127.5f);
        vertex_data[i].tangent[2] = static_cast<uint8_t>((vertices[i].tangent.z + 1.0f) * 127.5f);
        vertex_data[i].tangent[3] = static_cast<uint8_t>((vertices[i].tangent.w + 1.0f)
                                                         * 127.5f);  // Assuming .w is in [-1, 1]

        vertex_data[i].uv[0] = meshopt_quantizeHalf(vertices[i].uv.x);
        vertex_data[i].uv[1] = meshopt_quantizeHalf(vertices[i].uv.y);
      }

      std::vector<meshlet> meshlets = generate_meshlets(vertex_positions, indices);

      for (auto& idx : indices) {
        idx += initial_index;
      }

      for (auto& meshlet : meshlets) {
          //meshlet.vertex_count += initial_index;
      }
      mesh_surface surface = create_surface(vertex_positions, vertex_data, indices, std::move(meshlets));
      if (primitive.materialIndex.has_value()) {
        add_material_component(surface, material_offset + primitive.materialIndex.value());

      } else {
        add_material_component(surface, default_material);
      }
      surfaces.push_back(std::move(surface));
    }
    create_mesh(surfaces, std::string(mesh.name));
  }
}
