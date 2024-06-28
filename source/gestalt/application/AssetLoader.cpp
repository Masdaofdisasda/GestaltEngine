#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <meshoptimizer.h>

#include "SceneManager.hpp"

namespace gestalt::application {

    struct Vertex {
      glm::vec3 position;
      glm::vec3 normal;
      glm::vec4 tangent;
      glm::vec2 uv;
    };

    void AssetLoader::init(const std::shared_ptr<IResourceManager>& resource_manager,
                           const std::shared_ptr<ComponentFactory>& component_factory,
                           const std::shared_ptr<Repository>& repository) {
      resource_manager_ = resource_manager;
      repository_ = repository;
      component_factory_ = component_factory;
    }

    void AssetLoader::import_lights(const fastgltf::Asset& gltf) {
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

    std::vector<fastgltf::Node> AssetLoader::load_scene_from_gltf(const std::string& file_path) {
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

    VkFilter AssetLoader::extract_filter(fastgltf::Filter filter) {
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

    VkSamplerMipmapMode AssetLoader::extract_mipmap_mode(fastgltf::Filter filter) {
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

    std::optional<fastgltf::Asset> AssetLoader::parse_gltf(const std::filesystem::path& file_path) {
      static constexpr auto gltf_extensions
          = fastgltf::Extensions::KHR_lights_punctual
            | fastgltf::Extensions::KHR_materials_emissive_strength
            | fastgltf::Extensions::KHR_materials_clearcoat
            | fastgltf::Extensions::KHR_materials_ior | fastgltf::Extensions::KHR_materials_sheen
            | fastgltf::Extensions::None;

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

    void AssetLoader::import_samplers(fastgltf::Asset& gltf) {
      fmt::print("importing samplers\n");
      for (fastgltf::Sampler& sampler : gltf.samplers) {
        VkSamplerCreateInfo sampl
            = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode
            = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler new_sampler = resource_manager_->create_sampler(sampl);

        fmt::print("created sampler {}, sampler_id {}\n", sampler.name,
                   repository_->samplers.size());
        repository_->samplers.add(new_sampler);
      }
    }

    std::optional<TextureHandle> AssetLoader::load_image(fastgltf::Asset& asset,
                                                          fastgltf::Image& image) const {
      TextureHandle newImage = {};
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
              unsigned char* data = stbi_load_from_memory(vector.bytes.data(),
                                                          static_cast<int>(vector.bytes.size()),
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

    void AssetLoader::import_textures(fastgltf::Asset& gltf) const {
      fmt::print("importing textures\n");
      for (fastgltf::Image& image : gltf.images) {
        std::optional<TextureHandle> img = load_image(gltf, image);

        if (img.has_value()) {
          size_t image_id = repository_->textures.add(img.value());
          fmt::print("loaded texture {}, image_id {}\n", image.name, image_id);
        } else {
          fmt::print("gltf failed to load texture {}\n", image.name);
          repository_->textures.add(repository_->default_material_.error_checkerboard_image);
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

    std::tuple<TextureHandle, VkSampler> AssetLoader::get_textures(
        const fastgltf::Asset& gltf, const size_t& texture_index, const size_t& image_offset,
        const size_t& sampler_offset) const {
      const size_t image_index = gltf.textures[texture_index].imageIndex.value();
      const size_t sampler_index = gltf.textures[texture_index].samplerIndex.value();

      TextureHandle image = repository_->textures.get(image_index + image_offset);
      VkSampler sampler = repository_->samplers.get(sampler_index + sampler_offset);
      return std::make_tuple(image, sampler);
    }

    void AssetLoader::import_albedo(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                    const size_t& image_offset, const fastgltf::Material& mat,
                                    PbrMaterial& pbr_config) const {
      if (mat.pbrData.baseColorTexture.has_value()) {
        pbr_config.constants.flags |= kAlbedoTextureFlag;
        pbr_config.constants.albedo_tex_index = 0;
        auto [image, sampler] = get_textures(
            gltf, mat.pbrData.baseColorTexture.value().textureIndex, image_offset, sampler_offset);

        pbr_config.textures.albedo_image = image;
        pbr_config.textures.albedo_sampler = sampler;
      } else {
        pbr_config.constants.albedo_color
            = glm::vec4(mat.pbrData.baseColorFactor.at(0), mat.pbrData.baseColorFactor.at(1),
                        mat.pbrData.baseColorFactor.at(2), mat.pbrData.baseColorFactor.at(3));
      }
    }

    void AssetLoader::import_metallic_roughness(const fastgltf::Asset& gltf,
                                                const size_t& sampler_offset,
                                                const size_t& image_offset,
                                                const fastgltf::Material& mat,
                                                PbrMaterial& pbr_config) const {
      if (mat.pbrData.metallicRoughnessTexture.has_value()) {
        pbr_config.constants.flags |= kMetalRoughTextureFlag;
        pbr_config.constants.metal_rough_tex_index = 0;
        auto [image, sampler]
            = get_textures(gltf, mat.pbrData.metallicRoughnessTexture.value().textureIndex,
                           image_offset, sampler_offset);

        pbr_config.textures.metal_rough_image = image;
        assert(image.image != repository_->default_material_.error_checkerboard_image.image);
      } else {
        pbr_config.constants.metal_rough_factor
            = glm::vec2(mat.pbrData.roughnessFactor,
                        mat.pbrData.metallicFactor);  // r - occlusion, g - roughness, b - metallic
      }
    }

    void AssetLoader::import_normal(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                    const size_t& image_offset, const fastgltf::Material& mat,
                                    PbrMaterial& pbr_config) const {
      if (mat.normalTexture.has_value()) {
        pbr_config.constants.flags |= kNormalTextureFlag;
        pbr_config.constants.normal_tex_index = 0;
        auto [image, sampler] = get_textures(gltf, mat.normalTexture.value().textureIndex,
                                             image_offset, sampler_offset);

        pbr_config.textures.normal_image = image;
        pbr_config.textures.normal_sampler = sampler;
        // pbr_config.normal_scale = 1.f;  // TODO
      }
    }

    void AssetLoader::import_emissive(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                      const size_t& image_offset, const fastgltf::Material& mat,
                                      PbrMaterial& pbr_config) const {
      if (mat.emissiveTexture.has_value()) {
        pbr_config.constants.flags |= kEmissiveTextureFlag;
        pbr_config.constants.emissive_tex_index = 0;
        auto [image, sampler] = get_textures(gltf, mat.emissiveTexture.value().textureIndex,
                                             image_offset, sampler_offset);

        pbr_config.textures.emissive_image = image;
        pbr_config.textures.emissive_sampler = sampler;
      } else {
        const glm::vec3 emissiveColor = glm::vec3(mat.emissiveFactor.at(0), mat.emissiveFactor.at(1),
                                                  mat.emissiveFactor.at(2));
        if (length(emissiveColor) > 0) {
          pbr_config.constants.emissiveColor = emissiveColor;
          pbr_config.constants.emissiveStrength = mat.emissiveStrength;
        }
      }
    }

    void AssetLoader::import_occlusion(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                       const size_t& image_offset, const fastgltf::Material& mat,
                                       PbrMaterial& pbr_config) const {
      if (mat.occlusionTexture.has_value()) {
        pbr_config.constants.flags |= kOcclusionTextureFlag;
        pbr_config.constants.occlusion_tex_index = 0;
        auto [image, sampler] = get_textures(gltf, mat.occlusionTexture.value().textureIndex,
                                             image_offset, sampler_offset);

        pbr_config.textures.occlusion_image = image;
        pbr_config.textures.occlusion_sampler = sampler;
        pbr_config.constants.occlusionStrength = 1.f;
      }
    }

    void AssetLoader::import_material(fastgltf::Asset& gltf, size_t& sampler_offset,
                                      size_t& image_offset, fastgltf::Material& mat) const {
      PbrMaterial pbr_config{};

      if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
        pbr_config.transparent = true;
      } else if (mat.alphaMode == fastgltf::AlphaMode::Mask) {
        pbr_config.constants.alpha_cutoff = mat.alphaCutoff;
      }

      auto& default_material = repository_->default_material_;

      pbr_config.textures = {.albedo_image = default_material.color_image,
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

    void AssetLoader::import_materials(fastgltf::Asset& gltf, size_t& sampler_offset,
                                       size_t& image_offset) const {
      fmt::print("importing materials\n");
      for (fastgltf::Material& mat : gltf.materials) {
        if (mat.name == "arch.002") {
          mat.name = "material_" + std::to_string(repository_->materials.size());
        }
        import_material(gltf, sampler_offset, image_offset, mat);
      }
    }

    void AssetLoader::optimize_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
      // Step 1: Generate a remap table for vertex optimization
      std::vector<unsigned int> remap(vertices.size());
      size_t vertex_count
          = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(),
                                        vertices.data(), vertices.size(), sizeof(Vertex));

      // Step 2: Remap indices
      std::vector<uint32_t> remappedIndices(indices.size());
      meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(),
                               remap.data());

      // Step 3: Remap vertices
      std::vector<Vertex> remappedVertices(vertices.size());
      meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertex_count,
                                sizeof(Vertex), remap.data());

      // Step 4: Vertex cache optimization
      meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(),
                                  remappedIndices.size(), vertex_count);

      // Step 5: Overdraw optimization (optional, depending on your needs)
      meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(),
                               remappedIndices.size(),
                               reinterpret_cast<const float*>(remappedVertices.data()),
                               vertex_count, sizeof(Vertex), 1.05f);

      // Step 6: Vertex fetch optimization
      meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(),
                                  remappedIndices.size(), remappedVertices.data(), vertex_count,
                                  sizeof(Vertex));

      // Replace the original vectors with the optimized ones
      vertices.swap(remappedVertices);
      indices.swap(remappedIndices);
    }

    void AssetLoader::simplify_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
      const size_t index_count = indices.size();
      const size_t vertex_count = vertices.size();

      float complexityThreshold = 0.5f;  // 50% of the original complexity
      size_t target_index_count = index_count * complexityThreshold;
      float target_error = 1e-2f;
      unsigned int options = 0;

      std::vector<uint32_t> lod_indices(index_count);
      float lod_error = 0.0f;

      lod_indices.resize(meshopt_simplify(lod_indices.data(), indices.data(), index_count,
                                          reinterpret_cast<const float*>(vertices.data()),
                                          vertex_count, sizeof(Vertex), target_index_count,
                                          target_error, options, &lod_error));
      indices.swap(lod_indices);
    }

    std::vector<Meshlet> AssetLoader::generate_meshlets(std::vector<GpuVertexPosition>& vertices,
                                                        std::vector<uint32_t>& indices,
        std::vector<uint32>& meshlet_vertices,
                                                        std::vector<uint8>& meshlet_indices,
                                                        size_t global_surfaces_count) {
      constexpr size_t max_vertices = 64;
      constexpr size_t max_triangles = 64;  // Must be divisible by 4
      constexpr float cone_weight = 0.5f;    // Set between 0 and 1 if using cone culling

      const size_t global_meshlet_vertex_offset = repository_->meshlet_vertices.size();
      const size_t global_meshlet_index_offset = repository_->meshlet_triangles.size();

      // Compute the maximum number of meshlets
      size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);

      // Allocate buffers for meshopt meshlets, vertices, and triangles
      std::vector<meshopt_Meshlet> meshopt_meshlets(max_meshlets);
      std::vector<unsigned int> meshlet_vertices_local(max_meshlets * max_vertices);
      std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

      // Build the meshlets
      const size_t meshlet_count = meshopt_buildMeshlets(
          meshopt_meshlets.data(), meshlet_vertices_local.data(), meshlet_triangles.data(),
          indices.data(), indices.size(), reinterpret_cast<const float*>(vertices.data()),
          vertices.size(), sizeof(GpuVertexPosition), max_vertices, max_triangles, cone_weight);

      // Resize the arrays based on actual meshlet count and last meshlet's offsets + counts
      const auto& last = meshopt_meshlets[meshlet_count - 1];
      meshlet_vertices_local.resize(last.vertex_offset + last.vertex_count);
      meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
      meshopt_meshlets.resize(meshlet_count);

      // Prepare the result meshlets
      std::vector<Meshlet> result_meshlets(meshlet_count);

      for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& local_meshlet = meshopt_meshlets[i];
        auto& meshlet = result_meshlets[i];

        /** TODO update meshopt and do Meshlet optimization **/
        /*meshopt_optimizeMeshlet(&meshlet_vertices[meshlet.vertex_offset],
                                &meshlet_triangles[meshlet.triangle_offset], meshlet.triangle_count,
                                meshlet.vertex_count);*/

        // Compute the bounds for the current meshlet
        meshopt_Bounds meshlet_bounds = meshopt_computeMeshletBounds(
            meshlet_vertices_local.data() + local_meshlet.vertex_offset,
            meshlet_triangles.data() + local_meshlet.triangle_offset, local_meshlet.triangle_count,
            reinterpret_cast<float*>(vertices.data()), vertices.size(), sizeof(GpuVertexPosition));

        // Set meshlet data
        meshlet.vertex_offset = local_meshlet.vertex_offset + global_meshlet_vertex_offset;
        meshlet.index_offset = local_meshlet.triangle_offset + global_meshlet_index_offset;
        meshlet.vertex_count = local_meshlet.vertex_count;
        meshlet.triangle_count = local_meshlet.triangle_count;

        meshlet.center = glm::vec3{meshlet_bounds.center[0], meshlet_bounds.center[1],
                                   meshlet_bounds.center[2]};
        meshlet.radius = meshlet_bounds.radius;

        meshlet.cone_axis[0] = meshlet_bounds.cone_axis_s8[0];
        meshlet.cone_axis[1] = meshlet_bounds.cone_axis_s8[1];
        meshlet.cone_axis[2] = meshlet_bounds.cone_axis_s8[2];

        meshlet.cone_cutoff = meshlet_bounds.cone_cutoff_s8;
        // a meshlet belongs not to a mesh but to a surface, because we can have multiple surfaces
        // per mesh in order to use different materials. A MeshDraw dictates how to draw a surface with a given material
        meshlet.mesh_draw_index = static_cast<uint32>(global_surfaces_count);  
      }

      meshlet_indices.reserve(meshlet_triangles.size());
      for (const auto& idx : meshlet_triangles) {
        meshlet_indices.push_back(idx);
      }

      meshlet_vertices.reserve(meshlet_vertices_local.size());
      for (const auto& idx : meshlet_vertices_local) {
                meshlet_vertices.push_back(idx);
      }

      return result_meshlets;
    }

    MeshSurface AssetLoader::create_surface(std::vector<GpuVertexPosition>& vertex_positions,
                                            std::vector<GpuVertexData>& vertex_data,
                                            std::vector<uint32_t>& indices,
                                            std::vector<Meshlet>&& meshlets,
        std::vector<uint32>&& meshlet_vertices,
                                            std::vector<uint8>&& meshlet_indices) const {
      assert(!vertex_positions.empty() && !indices.empty());
      assert(vertex_positions.size() == vertex_data.size());

      const size_t vertex_count = vertex_positions.size();
      const size_t index_count = indices.size();

      glm::vec3 center(0.0f);

      for (const auto& vertex : vertex_positions) {
        center += vertex.position;
      }
      center /= static_cast<float>(vertex_positions.size());

      float radius = 0.0f;
      for (const auto& vertex : vertex_positions) {
        float distance = glm::distance(center, vertex.position);
        radius = std::max(radius, distance);
      }

      auto meshlet_count = meshlets.size();
      auto meshlet_offset = repository_->meshlets.size();
      repository_->meshlets.add(meshlets);
      repository_->meshlet_vertices.add(meshlet_vertices);
      repository_->meshlet_triangles.add(meshlet_indices);
      const auto mesh_draw = repository_->mesh_draws.add(MeshDraw{});

      const MeshSurface surface{
          .meshlet_offset = static_cast<uint32>(meshlet_offset),
          .meshlet_count = static_cast<uint32>(meshlet_count),
          .vertex_count = static_cast<uint32>(vertex_count),
          .index_count = static_cast<uint32>(index_count),
          .first_index = static_cast<uint32>(repository_->indices.size()),
          .vertex_offset = static_cast<uint32>(repository_->vertex_positions.size()),
          .local_bounds = BoundingSphere{center, radius},
          .mesh_draw = mesh_draw,    
      };

      repository_->vertex_positions.add(vertex_positions);
      repository_->vertex_data.add(vertex_data);
      const uint32 highest_index = *std::ranges::max_element(indices);
      assert(highest_index <= repository_->vertex_positions.size());
      repository_->indices.add(indices);

      return surface;
    }

    size_t AssetLoader::create_mesh(std::vector<MeshSurface> surfaces,
                                    const std::string& name) const {
      size_t mesh_id = repository_->meshes.size();
      const std::string key = name.empty() ? "mesh_" + std::to_string(mesh_id) : name;

      glm::vec3 combined_center(0.0f);
      for (const auto& surface : surfaces) {
        combined_center += surface.local_bounds.center;
      }
      combined_center /= static_cast<float>(surfaces.size());

      float combined_radius = 0.0f;
      for (const auto& surface : surfaces) {
        float distance = glm::distance(combined_center, surface.local_bounds.center)
                         + surface.local_bounds.radius;
        combined_radius = std::max(combined_radius, distance);
      }

      mesh_id = repository_->meshes.add(Mesh{key, std::move(surfaces), BoundingSphere{combined_center, combined_radius}});
      fmt::print("created mesh {}, mesh_id {}\n", key, mesh_id);
      return mesh_id;
    }

    void AssetLoader::add_material_component(MeshSurface& surface, const size_t material) const {
      assert(material != no_component);
      assert(material <= repository_->materials.size());
      assert(surface.material == default_material);
      surface.material = material;
    }

    void AssetLoader::import_meshes(fastgltf::Asset& gltf, const size_t material_offset) {
      std::vector<MeshSurface> surfaces;
      size_t global_surfaces_count = 0;

      fmt::print("importing meshes\n");
      for (fastgltf::Mesh& mesh : gltf.meshes) {
        surfaces.clear();

        for (auto&& primitive : mesh.primitives) {
          size_t global_index_offset = repository_->vertex_positions.size();
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

            vertex_data[i].tangent[0]
                = static_cast<uint8_t>((vertices[i].tangent.x + 1.0f) * 127.5f);
            vertex_data[i].tangent[1]
                = static_cast<uint8_t>((vertices[i].tangent.y + 1.0f) * 127.5f);
            vertex_data[i].tangent[2]
                = static_cast<uint8_t>((vertices[i].tangent.z + 1.0f) * 127.5f);
            vertex_data[i].tangent[3] = static_cast<uint8_t>(
                (vertices[i].tangent.w + 1.0f) * 127.5f);  // Assuming .w is in [-1, 1]

            vertex_data[i].uv[0] = meshopt_quantizeHalf(vertices[i].uv.x);
            vertex_data[i].uv[1] = meshopt_quantizeHalf(vertices[i].uv.y);
          }

          std::vector<uint32> meshlet_vertices;
          std::vector<uint8> meshlet_indices;
          std::vector<Meshlet> meshlets = generate_meshlets(
              vertex_positions, indices, meshlet_vertices, meshlet_indices, global_surfaces_count);

          for (auto& idx : indices) {
            idx += global_index_offset;
          }

          MeshSurface surface = create_surface(vertex_positions, vertex_data, indices,
                                               std::move(meshlets), std::move(meshlet_vertices),
                                               std::move(meshlet_indices));
          if (primitive.materialIndex.has_value()) {
            add_material_component(surface, material_offset + primitive.materialIndex.value());

          } else {
            add_material_component(surface, default_material);
          }
          surfaces.push_back(surface);
          global_surfaces_count++;
        }
        create_mesh(surfaces, std::string(mesh.name));
      }
    }
}  // namespace gestalt