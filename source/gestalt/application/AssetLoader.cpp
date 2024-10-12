#include <iso646.h>
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <meshoptimizer.h>
#include <fmt/core.h>

#include <functional>

#include "EntityManager.hpp"
#include "Animation/InterpolationType.hpp"
#include "Interface/IResourceManager.hpp"
#include "Mesh/MeshSurface.hpp"

namespace gestalt::application {

  struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
  };

  class MeshProcessor {
  public:
    static void optimize_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
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

    static void simplify_mesh(const std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                              const size_t max_indices) {
      const size_t index_count = indices.size();

      if (index_count <= max_indices) {
        // No simplification needed
        return;
      }

      const size_t vertex_count = vertices.size();
      
        float complexity_threshold
          = static_cast<float>(max_indices) / static_cast<float>(index_count);
      complexity_threshold
          = std::min(complexity_threshold, 1.0f); 

      const size_t target_index_count = index_count * complexity_threshold;
      constexpr float target_error = 1e-2f;
      constexpr unsigned int options = 0;

      std::vector<uint32_t> lod_indices(index_count);
      float lod_error = 0.0f;

      lod_indices.resize(meshopt_simplify(lod_indices.data(), indices.data(), index_count,
                                          reinterpret_cast<const float*>(vertices.data()),
                                          vertex_count, sizeof(Vertex), target_index_count,
                                          target_error, options, &lod_error));
      indices.swap(lod_indices);
    }

    struct CompressedVertexData {
      std::vector<GpuVertexPosition> vertex_positions;
      std::vector<GpuVertexData> vertex_data;
    };

    static CompressedVertexData compress_vertex_data(const std::vector<Vertex>& vertices) {
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

      return {vertex_positions, vertex_data};
    }

    struct MeshletData {
      std::vector<uint32> meshlet_vertices;
      std::vector<uint8> meshlet_indices;
      std::vector<Meshlet> meshlets;
    };

    static std::vector<meshopt_Meshlet> BuildMeshlets(
        const std::vector<uint32_t>& indices, const std::vector<GpuVertexPosition>& vertices,
        std::vector<uint32_t>& meshlet_vertices_local, std::vector<uint8_t>& meshlet_triangles) {
      constexpr size_t max_vertices = 64;
      constexpr size_t max_triangles = 64;
      constexpr float cone_weight = 0.5f;

      size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);

      meshlet_vertices_local.resize(max_meshlets * max_vertices);
      meshlet_triangles.resize(max_meshlets * max_triangles * 3);

      std::vector<meshopt_Meshlet> meshopt_meshlets(max_meshlets);
      size_t meshlet_count = meshopt_buildMeshlets(
          meshopt_meshlets.data(), meshlet_vertices_local.data(), meshlet_triangles.data(),
          indices.data(), indices.size(), reinterpret_cast<const float*>(vertices.data()),
          vertices.size(), sizeof(GpuVertexPosition), max_vertices, max_triangles, cone_weight);

      meshopt_meshlets.resize(meshlet_count);
      meshlet_vertices_local.resize(meshopt_meshlets.back().vertex_offset
                                    + meshopt_meshlets.back().vertex_count);
      meshlet_triangles.resize(meshopt_meshlets.back().triangle_offset
                               + ((meshopt_meshlets.back().triangle_count * 3 + 3) & ~3));

      return meshopt_meshlets;
    }

    static std::vector<Meshlet> ComputeMeshletBounds(
                                                    const std::vector<meshopt_Meshlet>& meshopt_meshlets,
                                                    const std::vector<uint32_t>& meshlet_vertices_local,
                                                    const std::vector<uint8_t>& meshlet_triangles,
                                                    const std::vector<GpuVertexPosition>& vertices,
                                                    size_t global_meshlet_vertex_offset,
                                                    size_t global_meshlet_index_offset, size_t global_mesh_draw_count) {
      std::vector<Meshlet> result_meshlets(meshopt_meshlets.size());

      for (size_t i = 0; i < meshopt_meshlets.size(); ++i) {
        const auto& local_meshlet = meshopt_meshlets[i];
        auto& meshlet = result_meshlets[i];

        meshopt_Bounds meshlet_bounds = meshopt_computeMeshletBounds(
            meshlet_vertices_local.data() + local_meshlet.vertex_offset,
            meshlet_triangles.data() + local_meshlet.triangle_offset, local_meshlet.triangle_count,
            reinterpret_cast<const float*>(vertices.data()), vertices.size(),
            sizeof(GpuVertexPosition));

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

        meshlet.mesh_draw_index = static_cast<uint32_t>(global_mesh_draw_count);
      }
      return result_meshlets;
    }

    static std::vector<uint8_t> ConvertAndStoreIndices(
        const std::vector<uint8_t>& meshlet_triangles) {
      std::vector<uint8_t> meshlet_indices;
      meshlet_indices.reserve(meshlet_triangles.size());
      meshlet_indices.insert(meshlet_indices.end(), meshlet_triangles.begin(),
                             meshlet_triangles.end());
      return meshlet_indices;
    }

    static std::vector<uint32_t> ConvertAndStoreVertices(
        const std::vector<uint32_t>& meshlet_vertices_local) {
      std::vector<uint32_t> meshlet_vertices;
      meshlet_vertices.reserve(meshlet_vertices_local.size());
      meshlet_vertices.insert(meshlet_vertices.end(), meshlet_vertices_local.begin(),
                              meshlet_vertices_local.end());
      return meshlet_vertices;
    }


    static MeshletData generate_meshlets(std::vector<GpuVertexPosition>& vertices,
                                         const std::vector<uint32_t>& indices,
                                         size_t global_mesh_draw_count,
                                         size_t global_meshlet_vertex_offset,
                                         size_t global_meshlet_index_offset) {
      std::vector<uint32_t> meshlet_vertices_local;
      std::vector<uint8_t> meshlet_triangles;

      auto meshopt_meshlets
          = BuildMeshlets(indices, vertices, meshlet_vertices_local, meshlet_triangles);

      auto result_meshlets = ComputeMeshletBounds(meshopt_meshlets, meshlet_vertices_local,
                           meshlet_triangles, vertices, global_meshlet_vertex_offset, global_meshlet_index_offset, global_mesh_draw_count);

      std::vector<uint8_t> meshlet_indices = ConvertAndStoreIndices(meshlet_triangles);
      std::vector<uint32_t> meshlet_vertices = ConvertAndStoreVertices(meshlet_vertices_local);

      return {meshlet_vertices, meshlet_indices, result_meshlets};
    }

    static MeshSurface create_surface(std::vector<GpuVertexPosition>& vertex_positions,
                                      std::vector<GpuVertexData>& vertex_data,
                                      std::vector<uint32_t>& indices,
                                      std::vector<Meshlet>&& meshlets,
                                      std::vector<uint32>&& meshlet_vertices,
                                      std::vector<uint8>&& meshlet_indices,
                                      Repository* repository) {
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

      glm::vec3 min(FLT_MAX);
      glm::vec3 max(-FLT_MAX);

      for (const auto& vertex : vertex_positions) {
        min = glm::min(min, vertex.position);
        max = glm::max(max, vertex.position);
      }

      auto meshlet_count = meshlets.size();
      auto meshlet_offset = repository->meshlets.size();
      repository->meshlets.add(meshlets);
      repository->meshlet_vertices.add(meshlet_vertices);
      repository->meshlet_triangles.add(meshlet_indices);
      const auto mesh_draw = repository->mesh_draws.add(MeshDraw{});

      const MeshSurface surface{
          .meshlet_offset = static_cast<uint32>(meshlet_offset),
          .meshlet_count = static_cast<uint32>(meshlet_count),
          .vertex_count = static_cast<uint32>(vertex_count),
          .index_count = static_cast<uint32>(index_count),
          .first_index = static_cast<uint32>(repository->indices.size()),
          .vertex_offset = static_cast<uint32>(repository->vertex_positions.size()),
          .local_bounds = BoundingSphere{center, radius},
          .local_aabb = AABB{min, max},
          .mesh_draw = mesh_draw,
      };

      repository->vertex_positions.add(vertex_positions);
      repository->vertex_data.add(vertex_data);
      const uint32 highest_index = *std::ranges::max_element(indices);
      assert(highest_index <= repository->vertex_positions.size());
      repository->indices.add(indices);

      return surface;
    }
  };

  class GltfParser {
    static std::vector<uint32_t> extract_indices(const fastgltf::Asset& gltf,
                                                 fastgltf::Primitive& surface) {
      std::vector<uint32_t> indices;
      const fastgltf::Accessor& indexaccessor = gltf.accessors[surface.indicesAccessor.value()];
      indices.reserve(indices.size() + indexaccessor.count);

      fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                                               [&](std::uint32_t idx) { indices.push_back(idx); });
      return indices;
    }

    static std::vector<Vertex> extract_vertices(const fastgltf::Asset& gltf,
                                                fastgltf::Primitive& surface) {
      std::vector<Vertex> vertices;
      const fastgltf::Accessor& posAccessor
          = gltf.accessors[surface.findAttribute("POSITION")->second];
      vertices.resize(vertices.size() + posAccessor.count);

      fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                    [&](glm::vec3 v, size_t index) {
                                                      Vertex vtx;
                                                      vtx.position = v;
                                                      vertices[index] = vtx;
                                                    });

      const auto normals = surface.findAttribute("NORMAL");
      if (normals != surface.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, gltf.accessors[(*normals).second],
            [&](glm::vec3 v, size_t index) { vertices[index].normal = v; });
      }

      const auto tangents = surface.findAttribute("TANGENT");
      if (tangents != surface.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            gltf, gltf.accessors[(*tangents).second],
            [&](glm::vec4 v, size_t index) { vertices[index].tangent = v; });
      }

      const auto uv = surface.findAttribute("TEXCOORD_0");
      if (uv != surface.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            gltf, gltf.accessors[(*uv).second],
            [&](glm::vec2 v, size_t index) { vertices[index].uv = v; });
      }
      return vertices;
    }

    static MeshSurface extract_mesh_surface(const fastgltf::Asset& gltf, fastgltf::Primitive& primitive, size_t material_offset,
                                            Repository* repository) {

      std::vector<uint32_t> indices = extract_indices(gltf, primitive);
      std::vector<Vertex> vertices = extract_vertices(gltf, primitive);

      MeshProcessor::optimize_mesh(vertices, indices);
      MeshProcessor::simplify_mesh(vertices, indices, 8192);

      auto [vertex_positions, vertex_data] = MeshProcessor::compress_vertex_data(vertices);

      const size_t global_meshlet_vertex_offset = repository->meshlet_vertices.size();
      const size_t global_meshlet_index_offset = repository->meshlet_triangles.size();
      const size_t global_mesh_draw_count = repository->mesh_draws.size();
      auto [meshlet_vertices, meshlet_indices, meshlets] = MeshProcessor::generate_meshlets(
          vertex_positions, indices, global_mesh_draw_count, global_meshlet_vertex_offset,
          global_meshlet_index_offset);

      const size_t global_index_offset = repository->vertex_positions.size();
      for (auto& idx : indices) {
        idx += global_index_offset;
      }

      MeshSurface surface = MeshProcessor::create_surface(
          vertex_positions, vertex_data, indices, std::move(meshlets), std::move(meshlet_vertices),
          std::move(meshlet_indices), repository);
      if (primitive.materialIndex.has_value()) {
        surface.material = material_offset + primitive.materialIndex.value();
      } else {
        surface.material = default_material;
      }
      return surface;
    }

  public:

    static std::vector<MeshSurface> extract_mesh(const fastgltf::Asset& gltf, fastgltf::Mesh& mesh,
                             size_t material_offset, Repository* repository) {
      std::vector<MeshSurface> surfaces;
      surfaces.reserve(mesh.primitives.size());
      for (fastgltf::Primitive& primitive : mesh.primitives) {
        MeshSurface surface = extract_mesh_surface(
            gltf, primitive, material_offset, repository);
        surfaces.push_back(surface);
      }
      return surfaces;
    }

    static void create_nodes(fastgltf::Asset& gltf, const size_t& mesh_offset,
                             ComponentFactory* component_factory) {
      for (fastgltf::Node& node : gltf.nodes) {
        glm::vec3 position(0.f);
        glm::quat orientation(1.f, 0.f, 0.f, 0.f);
        float uniform_scale = 1.f;

        if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
          const auto& [translation, rotation, scale] = std::get<fastgltf::TRS>(node.transform);

          position = glm::vec3(translation[0], translation[1], translation[2]);
          orientation = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);
          uniform_scale = (scale[0] + scale[1] + scale[2]) / 3.f;  // handle non-uniform scale
        }

        const auto [entity, node_component] = component_factory->create_entity(
            std::string(node.name), position, orientation, uniform_scale);

        if (node.lightIndex.has_value()) {
          auto light = gltf.lights.at(node.lightIndex.value());

            if (light.type == fastgltf::LightType::Directional) {
            glm::vec3 direction = normalize(glm::vec3(0, 0, -1.f) * orientation);
            component_factory->create_directional_light(
                  glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)), light.intensity,
                direction, entity);

            } else if (light.type == fastgltf::LightType::Point) {
              float32 range = light.range.value_or(100.f);

              component_factory->create_point_light(
                  glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)),
                  light.intensity, position, range, entity);

            } else if (light.type == fastgltf::LightType::Spot) {
              // TODO not supported yet
            }
        }

        if (node.cameraIndex.has_value()) {
          fastgltf::Camera cam = gltf.cameras.at(node.cameraIndex.value());
          if (std::holds_alternative<fastgltf::Camera::Perspective>(cam.camera)) {
            const auto& perspective = std::get<fastgltf::Camera::Perspective>(cam.camera);
            const float32 aspect_ratio = perspective.aspectRatio.value_or(1.f);
            const float32 fov = perspective.yfov; // in radians
            const float32 near = perspective.znear;
            const float32 far = perspective.zfar.value_or(10000.f);
            component_factory->add_animation_camera(
                position, orientation, entity,
                PerspectiveProjectionData{fov, aspect_ratio, near, far});
          } else if (std::holds_alternative<fastgltf::Camera::Orthographic>(cam.camera)) {
            const auto& ortho = std::get<fastgltf::Camera::Orthographic>(cam.camera);
            const float32 xmag = ortho.xmag;
            const float32 ymag = ortho.ymag;
            const float32 near = ortho.znear;
            const float32 far = ortho.zfar;
            component_factory->add_animation_camera(
                position, orientation, entity,
                OrthographicProjectionData{xmag, ymag, near, far});
          }
        }

        if (node.meshIndex.has_value()) {
          component_factory->add_mesh_component(entity, mesh_offset + *node.meshIndex);
        }

      }
    }

    static void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset, Repository* repository) {
      for (int i = 0; i < nodes.size(); i++) {
        fastgltf::Node& node = nodes[i];
        Entity parent_entity = node_offset + i;
        auto& scene_object = repository->scene_graph.get(parent_entity).value().get();

        for (auto& c : node.children) {
          Entity child_entity = node_offset + c;
          auto& child = repository->scene_graph.get(child_entity).value().get();
          scene_object.children.push_back(child_entity);
          child.parent = parent_entity;
        }
      }
    }

    static void link_orphans_to_root(const Entity root, NodeComponent& root_node,
                                     std::unordered_map<Entity, NodeComponent>& nodes) {
      for (auto& [entity, node] : nodes) {
        if (entity == root) {
          continue;
        }

        if (node.parent == invalid_entity) {
          root_node.children.push_back(entity);
          node.parent = root;
        }
      }
    }

  };


  void AssetLoader::init(IResourceManager* resource_manager, ComponentFactory* component_factory,
                         Repository* repository) {
    resource_manager_ = resource_manager;
    repository_ = repository;
    component_factory_ = component_factory;
  }

  void AssetLoader::load_scene_from_gltf(const std::string& file_path) {
    fmt::print("Loading GLTF: {}\n", file_path);

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
              component_factory_->create_physics_component(
                  entity, DYNAMIC, BoxCollider{bounds});
            } else if (name == "ST_BO") {
              const glm::vec3 bounds = mesh.local_aabb.max - mesh.local_aabb.min;
              component_factory_->create_physics_component(
                  entity, STATIC, BoxCollider{bounds});
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

        // Depending on the target path (translation, rotation, or scale), load the appropriate keyframe data
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
        }
        else if (type == fastgltf::AnimationPath::Rotation) {
          rotation_keyframes.resize(input_accessor.count);

          fastgltf::iterateAccessorWithIndex<float>(
              gltf, input_accessor,
              [&](float time, size_t index) {
                rotation_keyframes[index].time = time;
              });

          fastgltf::iterateAccessorWithIndex<glm::vec4>(
              gltf, output_accessor,
              [&](glm::vec4 rotation, size_t index) {
                // NOTE: ordering of quaternion components is different in GLTF
                // also the coordinate system is different
                const auto rotationQuat = glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
                rotation_keyframes[index].value = glm::normalize(glm::conjugate(rotationQuat));
                rotation_keyframes[index].type = interpolation;
              });
        }
        else if (type == fastgltf::AnimationPath::Scale) {
          scale_keyframes.resize(input_accessor.count);

          fastgltf::iterateAccessorWithIndex<float>(
              gltf, input_accessor, [&](float time, size_t index) { scale_keyframes[index].time = time; });

          fastgltf::iterateAccessorWithIndex<glm::vec3>(
              gltf, output_accessor,
              [&](glm::vec3 scale, size_t index) {
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

  TextureHandle AssetLoader::get_textures(const fastgltf::Asset& gltf, const size_t& texture_index,
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
      assert(image.image != repository_->default_material_.error_checkerboard_image.image);
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

    pbr_config.textures = {.albedo_image = default_material.color_image,
                           .albedo_sampler = default_material.color_sampler,
                           .metal_rough_image = default_material.metallic_roughness_image,
                           .metal_rough_sampler = default_material.metallic_roughness_sampler,
                           .normal_image = default_material.normal_image,
                           .normal_sampler = default_material.normal_sampler,
                           .emissive_image = default_material.emissive_image,
                           .emissive_sampler = default_material.emissive_sampler,
                           .occlusion_image = default_material.occlusion_image,
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