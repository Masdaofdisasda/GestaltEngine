#include "asset_loader.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include "scene_manager.h"


void asset_loader::init(const vk_gpu& gpu,
                        const std::shared_ptr<resource_manager>& resource_manager) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;
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
  fastgltf::Parser parser{};
  constexpr auto gltf_options
      = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::None;

  fastgltf::GltfDataBuffer data;
  if (!data.loadFromFile(file_path)) {
    fmt::print("Failed to load file data from path: {}\n", file_path.string());
    return std::nullopt; 
  }

  std::filesystem::path path = file_path;

  auto type = determineGltfFileType(&data);
  if (type == fastgltf::GltfType::glTF) {
    auto loadResult = parser.loadGLTF(&data, path.parent_path(), gltf_options);
    if (loadResult) {
      return std::move(loadResult.get());
    }
    fmt::print("Failed to load glTF: {}\n", to_underlying(loadResult.error()));
  } else if (type == fastgltf::GltfType::GLB) {
    auto loadResult = parser.loadBinaryGLTF(&data, path.parent_path(), gltf_options);
    if (loadResult) {
      return std::move(loadResult.get()); 
    }
    fmt::print("Failed to load glTF: {}\n", to_underlying(loadResult.error()));
  } else {
    fmt::print("Failed to determine glTF container\n");
  }

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

    fmt::print("created sampler {}, sampler_id {}\n", sampler.name, resource_manager_->get_database().get_samplers_size());
    resource_manager_->get_database().add_sampler(new_sampler);
    //TODO deletion_service_.push(new_sampler);
  }
}

void asset_loader::import_textures(fastgltf::Asset& gltf) {
  fmt::print("importing textures\n");
  for (fastgltf::Image& image : gltf.images) {
    std::optional<AllocatedImage> img = resource_manager_->load_image(gltf, image);

    if (img.has_value()) {
      size_t image_id = resource_manager_->get_database().add_image(img.value());
      fmt::print("loaded texture {}, image_id {}\n", image.name, image_id);
    } else {
      fmt::print("gltf failed to load texture {}\n", image.name);
      resource_manager_->get_database().add_image(
          resource_manager_->get_database().default_material_.error_checkerboard_image);
    }
  }
}

size_t asset_loader::create_material(const gltf_material& material, const pbr_config& config,
                                      const std::string& name) const {
  const size_t material_id = resource_manager_->get_database().get_materials_size();
  fmt::print("creating material {}, mat_id {}\n", name, material_id);

  const std::string key = name.empty() ? "material_" + std::to_string(material_id) : name;
  resource_manager_->write_material(material, material_id);

  resource_manager_->get_database().add_material(material_component{.name = key, .config = config});

  return material_id;
}

void asset_loader::import_materials(fastgltf::Asset& gltf, size_t& material_offset) const {
  int data_index = static_cast<int>(material_offset);

  fmt::print("importing materials\n");
  for (fastgltf::Material& mat : gltf.materials) {
    pbr_config pbr_config{};
    gltf_material::MaterialConstants constants;
    constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
    constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
    constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
    constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

    constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
    constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

    if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
      pbr_config.transparent = true;
    }

    auto& default_material = resource_manager_->get_database().default_material_;

    gltf_material::MaterialResources materialResources
        = {.colorImage = default_material.color_image,
           .colorSampler = default_material.default_sampler_linear,
           .metalRoughImage = default_material.metallic_roughness_image,
           .metalRoughSampler = default_material.default_sampler_linear,
           .normalImage = default_material.normal_image,
           .normalSampler = default_material.default_sampler_linear,
           .emissiveImage = default_material.emissive_image,
           .emissiveSampler = default_material.default_sampler_linear,
           .occlusionImage = default_material.occlusion_image,
           .occlusionSampler = default_material.default_sampler_nearest};

    // grab textures from gltf file
    if (mat.pbrData.baseColorTexture.has_value()) {
      size_t img
          = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
      size_t sampler
          = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

      materialResources.colorImage = resource_manager_->get_database().get_image(img);
      materialResources.colorSampler = resource_manager_->get_database().get_sampler(sampler);
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

      materialResources.metalRoughImage = resource_manager_->get_database().get_image(img);
      materialResources.metalRoughSampler = resource_manager_->get_database().get_sampler(sampler);
      pbr_config.metal_rough_tex = true;
      pbr_config.metal_rough_factor
          = glm::vec2(mat.pbrData.metallicFactor, mat.pbrData.roughnessFactor);
      pbr_config.metal_rough_tex = img;
    }
    if (mat.normalTexture.has_value()) {
      size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
      size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();

      materialResources.normalImage = resource_manager_->get_database().get_image(img);
      materialResources.normalSampler = resource_manager_->get_database().get_sampler(sampler);
      pbr_config.use_normal_tex = true;
      pbr_config.normal_scale = 1.f; // TODO
      pbr_config.normal_tex = img;
    }
    if (mat.emissiveTexture.has_value()) {
      size_t img = gltf.textures[mat.emissiveTexture.value().textureIndex].imageIndex.value();
      size_t sampler = gltf.textures[mat.emissiveTexture.value().textureIndex].samplerIndex.value();

      materialResources.emissiveImage = resource_manager_->get_database().get_image(img);
      materialResources.emissiveSampler = resource_manager_->get_database().get_sampler(sampler);
      pbr_config.use_emissive_tex = true;
      pbr_config.emissive_factor
          = glm::vec3(mat.emissiveFactor.at(0), mat.emissiveFactor.at(1), mat.emissiveFactor.at(2));
      pbr_config.emissive_tex = img;
      pbr_config.emissive_factor; // TODO
    }
    if (mat.occlusionTexture.has_value()) {
      size_t img = gltf.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
      size_t sampler
          = gltf.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();

      materialResources.occlusionImage = resource_manager_->get_database().get_image(img);
      materialResources.occlusionSampler = resource_manager_->get_database().get_sampler(sampler);
      pbr_config.use_occlusion_tex = true;
      pbr_config.occlusion_tex = img;
      pbr_config.occlusion_strength = 1.f; // TODO
    }
    // build material
    create_material({constants, materialResources}, pbr_config, std::string(mat.name));
    data_index++;
  }
}

size_t asset_loader::create_surface(std::vector<Vertex>& vertices,
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
      .first_index = static_cast<uint32_t>(resource_manager_->get_database().get_indices_size()),
      .vertex_offset = static_cast<uint32_t>(resource_manager_->get_database().get_vertices_size()),
      .bounds = bounds,
  };

  resource_manager_->get_database().add_vertices(vertices);
  uint32_t highest_index = *std::max_element(indices.begin(), indices.end());
  assert(highest_index <= resource_manager_->get_database().get_vertices_size());
  resource_manager_->get_database().add_indices(indices);

  size_t surface_id = resource_manager_->get_database().add_surface(surface);
  fmt::print("created surface {}, index count {}\n", surface_id, surface.index_count);

  return surface_id;
}

size_t asset_loader::create_mesh(std::vector<size_t> surfaces, const std::string& name) const {
  size_t mesh_id = resource_manager_->get_database().get_meshes_size();
  const std::string key = name.empty() ? "mesh_" + std::to_string(mesh_id) : name;
  mesh_id = resource_manager_->get_database().add_mesh(mesh_component{key, std::move(surfaces)});
  fmt::print("created mesh {}, mesh_id {}\n", key, mesh_id);
  return mesh_id;
}

void asset_loader::add_material_component(const size_t surface, const size_t material) const {
  assert(material != no_component);
  assert(material <= resource_manager_->get_database().get_materials_size());
  assert(surface <= resource_manager_->get_database().get_surfaces_size());
  assert(resource_manager_->get_database().get_surface(surface).material == default_material);
  resource_manager_->get_database().get_surface(surface).material = material;
}

void asset_loader::import_meshes(fastgltf::Asset& gltf, const size_t material_offset) {
  std::vector<size_t> surfaces;

  fmt::print("importing meshes\n");
  for (fastgltf::Mesh& mesh : gltf.meshes) {
    surfaces.clear();

    for (auto&& primitive : mesh.primitives) {
      size_t initial_index = resource_manager_->get_database().get_vertices_size();
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
        add_material_component(surface_index,
                               material_offset + primitive.materialIndex.value());

      } else {
        add_material_component(surface_index, default_material);
      }
      surfaces.push_back(surface_index);
    }
    create_mesh(surfaces, std::string(mesh.name));
  }
  resource_manager_->upload_mesh();
}

std::vector<fastgltf::Node> asset_loader::load_scene_from_gltf(const std::string& file_path) {
  fmt::print("Loading GLTF: {}\n", file_path);

  fastgltf::Asset gltf = parse_gltf(file_path).value();

  import_samplers(gltf);
  import_textures(gltf);

  size_t material_offset = resource_manager_->get_database().get_materials_size();
  import_materials(gltf, material_offset);

  import_meshes(gltf, material_offset);

  return gltf.nodes;
}
