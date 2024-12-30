#include "AssetLoader.hpp"

#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <meshoptimizer.h>
#include <fmt/core.h>

#include <functional>
#include <ranges>

#include "GltfParser.hpp"
#include "ECS/ECSManager.hpp"
#include "Animation/InterpolationType.hpp"
#include "ECS/ComponentFactory.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Interface/IResourceManager.hpp"
#include "Mesh/MeshSurface.hpp"

namespace gestalt::application {
  void AssetLoader::init(IResourceManager* resource_manager, IResourceAllocator* resource_allocator,
                         ComponentFactory* component_factory,
                         Repository* repository) {
    resource_manager_ = resource_manager;
    resource_allocator_ = resource_allocator;
    repository_ = repository;
    component_factory_ = component_factory;
  }

  void AssetLoader::load_scene_from_gltf(const std::filesystem::path& file_path) {
    fmt::print("Loading GLTF: {}\n", file_path.string());

    const size_t node_offset = repository_->scene_graph.size();

    fastgltf::Asset gltf;
    if (auto asset = parse_gltf(file_path)) {
      gltf = std::move(asset.value());
    } else {
      return;
    }

    size_t image_offset = repository_->textures.size();
    const size_t material_offset = repository_->materials.size();

    import_nodes(gltf);

    import_meshes(gltf, material_offset);

    import_textures(gltf);

    import_materials(gltf, image_offset);

    import_animations(gltf, node_offset);

    {  // Import physics
      for (const auto& entity : repository_->scene_graph.components() | std::views::keys) {
        if (repository_->mesh_components.get(entity).has_value()) {
          const auto& mesh = repository_->meshes.get(repository_->mesh_components[entity].mesh);

          for (const auto& surface : mesh.surfaces) {
            auto name = repository_->materials.get(surface.material).name;
            if (name == "DY_SP") {
              component_factory_->create_physics_component(
                  entity, DYNAMIC, SphereCollider{mesh.local_bounds.radius});
            } else if (name == "DY_BO") {
              const glm::vec3 bounds = mesh.local_aabb.max - mesh.local_aabb.min;
              component_factory_->create_physics_component(entity, DYNAMIC, BoxCollider{bounds});
            } else if (name == "ST_BO") {
              const glm::vec3 bounds = mesh.local_aabb.max - mesh.local_aabb.min;
              component_factory_->create_physics_component(entity, STATIC, BoxCollider{bounds});
            } else if (name == "ST_SP") {
              component_factory_->create_physics_component(
                  entity, STATIC, SphereCollider{mesh.local_bounds.radius});
            }
          }
        }
      }
    }
  }

  InterpolationType MapInterpolationType(fastgltf::AnimationInterpolation type) {
    if (type == fastgltf::AnimationInterpolation::Linear) {
      return InterpolationType::kLinear;
    }
    if (type == fastgltf::AnimationInterpolation::Step) {
      return InterpolationType::kStep;
    }
    if (type == fastgltf::AnimationInterpolation::CubicSpline) {
      return InterpolationType::kCubic;
    }
    return InterpolationType::kLinear;
  }

  void AssetLoader::import_animations(const fastgltf::Asset& gltf, const size_t node_offset) {
    fmt::print("Importing animations\n");

    for (const fastgltf::Animation& animation : gltf.animations) {
      fmt::print("Importing animation: {}\n", animation.name);

      std::vector<Keyframe<glm::vec3>> translation_keyframes;
      std::vector<Keyframe<glm::quat>> rotation_keyframes;
      std::vector<Keyframe<glm::vec3>> scale_keyframes;
      Entity entity;

      for (auto& channel : animation.channels) {
        auto& sampler = animation.samplers[channel.samplerIndex];
        entity = channel.nodeIndex.value_or(0) + node_offset;
        auto& type = channel.path;
        auto interpolation = MapInterpolationType(sampler.interpolation);

        // Retrieve the input (keyframe times) and output (keyframe values) accessors
        const fastgltf::Accessor& input_accessor = gltf.accessors[sampler.inputAccessor];
        const fastgltf::Accessor& output_accessor = gltf.accessors[sampler.outputAccessor];

        // Depending on the target path (translation, rotation, or scale), load the appropriate
        // keyframe data
        if (type == fastgltf::AnimationPath::Translation) {
          translation_keyframes.resize(input_accessor.count);

          fastgltf::iterateAccessorWithIndex<float>(
              gltf, input_accessor,
              [&](float time, size_t index) { translation_keyframes[index].time = time; });

          fastgltf::iterateAccessorWithIndex<glm::vec3>(
              gltf, output_accessor, [&](glm::vec3 translation, size_t index) {
                translation_keyframes[index].value = translation;
                translation_keyframes[index].type = interpolation;
              });
        } else if (type == fastgltf::AnimationPath::Rotation) {
          rotation_keyframes.resize(input_accessor.count);

          fastgltf::iterateAccessorWithIndex<float>(
              gltf, input_accessor,
              [&](float time, size_t index) { rotation_keyframes[index].time = time; });

          fastgltf::iterateAccessorWithIndex<glm::vec4>(
              gltf, output_accessor, [&](glm::vec4 rotation, size_t index) {
                // NOTE: ordering of quaternion components is different in GLTF
                // also the coordinate system is different
                const auto rotationQuat = glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
                rotation_keyframes[index].value = glm::normalize(glm::conjugate(rotationQuat));
                rotation_keyframes[index].type = interpolation;
              });
        } else if (type == fastgltf::AnimationPath::Scale) {
          scale_keyframes.resize(input_accessor.count);

          fastgltf::iterateAccessorWithIndex<float>(
              gltf, input_accessor,
              [&](float time, size_t index) { scale_keyframes[index].time = time; });

          fastgltf::iterateAccessorWithIndex<glm::vec3>(
              gltf, output_accessor, [&](glm::vec3 scale, size_t index) {
                scale_keyframes[index].value = scale;
                scale_keyframes[index].type = interpolation;
              });
        }
      }
      component_factory_->create_animation_component(entity, translation_keyframes,
                                                     rotation_keyframes, scale_keyframes);
    }
  }

  void AssetLoader::import_nodes(fastgltf::Asset& gltf) const {
    const size_t mesh_offset = repository_->meshes.size();
    const size_t node_offset = repository_->scene_graph.size();

    GltfParser::create_nodes(gltf, mesh_offset, component_factory_);
    GltfParser::build_hierarchy(gltf.nodes, node_offset, repository_);
    constexpr Entity root = 0;
    GltfParser::link_orphans_to_root(root, repository_->scene_graph.get(root).value().get(),
                                     repository_->scene_graph.components());
  }

  std::optional<fastgltf::Asset> AssetLoader::parse_gltf(const std::filesystem::path& file_path) {
    static constexpr auto gltf_extensions
        = fastgltf::Extensions::KHR_lights_punctual
          | fastgltf::Extensions::KHR_materials_emissive_strength
          | fastgltf::Extensions::KHR_materials_clearcoat | fastgltf::Extensions::KHR_materials_ior
          | fastgltf::Extensions::KHR_materials_sheen | fastgltf::Extensions::None;

    fastgltf::Parser parser{gltf_extensions};
    constexpr auto gltf_options
        = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble
          | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers
          | fastgltf::Options::LoadExternalImages | fastgltf::Options::DecomposeNodeMatrices
          | fastgltf::Options::None;

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

  std::shared_ptr<ImageInstance> AssetLoader::load_image(
      fastgltf::Asset& asset,
                                                       fastgltf::Image& image) const {

    std::string image_name = image.name.c_str();
    if (image_name.empty()) {
      image_name = "Image " + std::to_string(asset.images.size());
    }
    ImageTemplate image_template(image_name);

    const std::function<void(fastgltf::sources::URI&)> create_image_from_file
        = [&](fastgltf::sources::URI& file_path) {
            fmt::print("Loading image from file: {}\n", file_path.uri.string());
            if (file_path.uri.string().empty()) {
              throw std::runtime_error("Empty file path");
            }
            if (file_path.fileByteOffset != 0 || !file_path.uri.isLocalPath()) {
              throw std::runtime_error("Image file offsets and external files are not supported!");
            }

            const auto image_path = std::filesystem::path(file_path.uri.path().begin(),
                                                          file_path.uri.path().end());
            image_template.set_initial_value(image_path);
          };

    const std::function<void(fastgltf::sources::Array&)> create_image_from_vector
        = [&](fastgltf::sources::Array& vector) {
      image_template.set_initial_value(vector.bytes.data(), vector.bytes.size());
                };

    const std::function<void(fastgltf::sources::BufferView&)> create_image_from_buffer_view
        = [&](fastgltf::sources::BufferView& view) {
            const auto& buffer_view = asset.bufferViews[view.bufferViewIndex];
            auto& buffer = asset.buffers[buffer_view.bufferIndex];

            std::visit(fastgltf::visitor{[](auto& arg) {},
                                         [&](fastgltf::sources::Array& vector) {
                                           image_template.set_initial_value(
                                               vector.bytes.data() + buffer_view.byteOffset,
                                               buffer_view.byteLength);
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

    auto image_instance = resource_allocator_->create_image(std::move(image_template));

    return image_instance;
  }

  void AssetLoader::import_textures(fastgltf::Asset& gltf) const {
    fmt::print("importing textures\n");
    for (fastgltf::Image& image : gltf.images) {
      auto img = load_image(gltf, image);

      if (img->get_image_handle() != VK_NULL_HANDLE) {
        size_t image_id = repository_->textures.add(img);
        fmt::print("loaded texture {}, image_id {}\n", image.name, image_id);
      } else {
        fmt::print("gltf failed to load texture {}\n", image.name);
        repository_->textures.add(repository_->default_material_.error_checkerboard_image_instance);
      }
    }
  }

  size_t AssetLoader::create_material(const PbrMaterial& config, const std::string& name) const {
    const size_t material_id = repository_->materials.size();
    fmt::print("creating material {}, mat_id {}\n", name, material_id);

    const std::string key = name.empty() ? "material_" + std::to_string(material_id) : name;

    repository_->materials.add(Material{.name = key, .config = config});

    return material_id;
  }

  std::shared_ptr<ImageInstance> AssetLoader::get_textures(const fastgltf::Asset& gltf,
                                                           const size_t& texture_index,
                                          const size_t& image_offset) const {
    const size_t image_index = gltf.textures[texture_index].imageIndex.value();
    const size_t sampler_index = gltf.textures[texture_index].samplerIndex.value();

    return repository_->textures.get(image_index + image_offset);
  }

  void AssetLoader::import_albedo(const fastgltf::Asset& gltf, const size_t& image_offset,
                                  const fastgltf::Material& mat, PbrMaterial& pbr_config) const {
    if (mat.pbrData.baseColorTexture.has_value()) {
      pbr_config.constants.flags |= kAlbedoTextureFlag;
      pbr_config.constants.albedo_tex_index = 0;
      const auto image
          = get_textures(gltf, mat.pbrData.baseColorTexture.value().textureIndex, image_offset);

      pbr_config.textures.albedo_image = image;
    } else {
      pbr_config.constants.albedo_color
          = glm::vec4(mat.pbrData.baseColorFactor.at(0), mat.pbrData.baseColorFactor.at(1),
                      mat.pbrData.baseColorFactor.at(2), mat.pbrData.baseColorFactor.at(3));
    }
  }

  void AssetLoader::import_metallic_roughness(const fastgltf::Asset& gltf,
                                              const size_t& image_offset,
                                              const fastgltf::Material& mat,
                                              PbrMaterial& pbr_config) const {
    if (mat.pbrData.metallicRoughnessTexture.has_value()) {
      pbr_config.constants.flags |= kMetalRoughTextureFlag;
      pbr_config.constants.metal_rough_tex_index = 0;
      const auto image = get_textures(
          gltf, mat.pbrData.metallicRoughnessTexture.value().textureIndex, image_offset);

      pbr_config.textures.metal_rough_image = image;
    } else {
      pbr_config.constants.metal_rough_factor
          = glm::vec2(mat.pbrData.roughnessFactor,
                      mat.pbrData.metallicFactor);  // r - occlusion, g - roughness, b - metallic
    }
  }

  void AssetLoader::import_normal(const fastgltf::Asset& gltf, const size_t& image_offset,
                                  const fastgltf::Material& mat, PbrMaterial& pbr_config) const {
    if (mat.normalTexture.has_value()) {
      pbr_config.constants.flags |= kNormalTextureFlag;
      pbr_config.constants.normal_tex_index = 0;
      const auto image = get_textures(gltf, mat.normalTexture.value().textureIndex, image_offset);

      pbr_config.textures.normal_image = image;
      // pbr_config.normal_scale = 1.f;  // TODO
    }
  }

  void AssetLoader::import_emissive(const fastgltf::Asset& gltf, const size_t& image_offset,
                                    const fastgltf::Material& mat, PbrMaterial& pbr_config) const {
    if (mat.emissiveTexture.has_value()) {
      pbr_config.constants.flags |= kEmissiveTextureFlag;
      pbr_config.constants.emissive_tex_index = 0;
      const auto image = get_textures(gltf, mat.emissiveTexture.value().textureIndex, image_offset);

      pbr_config.textures.emissive_image = image;
    } else {
      const glm::vec3 emissiveColor
          = glm::vec3(mat.emissiveFactor.at(0), mat.emissiveFactor.at(1), mat.emissiveFactor.at(2));
      if (length(emissiveColor) > 0) {
        pbr_config.constants.emissiveColor = emissiveColor;
        pbr_config.constants.emissiveStrength = mat.emissiveStrength;
      }
    }
  }

  void AssetLoader::import_occlusion(const fastgltf::Asset& gltf, const size_t& image_offset,
                                     const fastgltf::Material& mat, PbrMaterial& pbr_config) const {
    if (mat.occlusionTexture.has_value()) {
      pbr_config.constants.flags |= kOcclusionTextureFlag;
      pbr_config.constants.occlusion_tex_index = 0;
      const auto image
          = get_textures(gltf, mat.occlusionTexture.value().textureIndex, image_offset);

      pbr_config.textures.occlusion_image = image;
      pbr_config.constants.occlusionStrength = 1.f;
    }
  }

  void AssetLoader::import_material(fastgltf::Asset& gltf, size_t& image_offset,
                                    fastgltf::Material& mat) const {
    PbrMaterial pbr_config{};

    if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
      pbr_config.transparent = true;
    } else if (mat.alphaMode == fastgltf::AlphaMode::Mask) {
      pbr_config.constants.alpha_cutoff = mat.alphaCutoff;
    }

    auto& default_material = repository_->default_material_;

    pbr_config.textures = {.albedo_image = default_material.color_image_instance,
                           .albedo_sampler = default_material.color_sampler,
                           .metal_rough_image = default_material.metallic_roughness_image_instance,
                           .metal_rough_sampler = default_material.metallic_roughness_sampler,
                           .normal_image = default_material.normal_image_instance,
                           .normal_sampler = default_material.normal_sampler,
                           .emissive_image = default_material.emissive_image_instance,
                           .emissive_sampler = default_material.emissive_sampler,
                           .occlusion_image = default_material.occlusion_image_instance,
                           .occlusion_sampler = default_material.occlusion_sampler};

    // grab textures from gltf file
    import_albedo(gltf, image_offset, mat, pbr_config);
    import_metallic_roughness(gltf, image_offset, mat, pbr_config);
    import_normal(gltf, image_offset, mat, pbr_config);
    import_emissive(gltf, image_offset, mat, pbr_config);
    import_occlusion(gltf, image_offset, mat, pbr_config);

    // build material
    create_material(pbr_config, std::string(mat.name));
  }

  void AssetLoader::import_materials(fastgltf::Asset& gltf, size_t& image_offset) const {
    fmt::print("importing materials\n");
    for (fastgltf::Material& mat : gltf.materials) {
      import_material(gltf, image_offset, mat);
    }
  }

  void AssetLoader::import_meshes(fastgltf::Asset& gltf, const size_t material_offset) const {
    fmt::print("importing meshes\n");
    for (fastgltf::Mesh& mesh : gltf.meshes) {
      const auto surfaces = GltfParser::extract_mesh(gltf, mesh, material_offset, repository_);

      component_factory_->create_mesh(surfaces, std::string(mesh.name));
    }
  }
}  // namespace gestalt::application