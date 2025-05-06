#include "GltfParser.hpp"

#include <algorithm>
#include <fastgltf/glm_element_traits.hpp>

#include <fmt/core.h>

#include <functional>
#include <ranges>

#include "MeshProcessor.hpp"
#include "ECS/EntityComponentSystem.hpp"
#include "ECS/ComponentFactory.hpp"
#include "Mesh/MeshSurface.hpp"

constexpr auto forward_z = glm::vec3(0, 0, -1.f);

static glm::vec3 compute_direction(const glm::quat orientation) { return glm::normalize(orientation * forward_z); }

namespace gestalt::application {

  std::vector<uint32_t> GltfParser::extract_indices(const fastgltf::Asset& gltf,
      fastgltf::Primitive& surface) {
    std::vector<uint32_t> indices;
    const fastgltf::Accessor& indexaccessor = gltf.accessors[surface.indicesAccessor.value()];
    indices.reserve(indices.size() + indexaccessor.count);

    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                                             [&](std::uint32_t idx) { indices.push_back(idx); });
    return indices;
  }

  std::vector<Vertex> GltfParser::extract_vertices(
      const fastgltf::Asset& gltf, fastgltf::Primitive& surface) {
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

  MeshSurface GltfParser::extract_mesh_surface(const fastgltf::Asset& gltf,
      fastgltf::Primitive& primitive, size_t material_offset, Repository* repository) {
    std::vector<uint32_t> indices = extract_indices(gltf, primitive);
    std::vector<Vertex> vertices = extract_vertices(gltf, primitive);

    MeshProcessor::optimize_mesh(vertices, indices);
    //MeshProcessor::simplify_mesh(vertices, indices, 8192);

    auto [vertex_positions, vertex_data] = MeshProcessor::compress_vertex_data(vertices);

    const size_t global_meshlet_vertex_offset = repository->meshlet_vertices.size();
    const size_t global_meshlet_index_offset = repository->meshlet_triangles.size();
    const size_t global_mesh_draw_count = repository->mesh_draws.size();
    auto [meshlet_vertices, meshlet_indices, meshlets] = MeshProcessor::generate_meshlets(
        vertex_positions, indices, global_mesh_draw_count, global_meshlet_vertex_offset,
        global_meshlet_index_offset);

    const size_t global_index_offset = repository->vertex_positions.size();
    for (auto& idx : indices) {
      idx += static_cast<uint32>(global_index_offset);
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

  std::vector<MeshSurface> GltfParser::extract_mesh(const fastgltf::Asset& gltf,
      fastgltf::Mesh& mesh, size_t material_offset, Repository* repository) {
    std::vector<MeshSurface> surfaces;
    surfaces.reserve(mesh.primitives.size());
    for (fastgltf::Primitive& primitive : mesh.primitives) {
      MeshSurface surface = extract_mesh_surface(gltf, primitive, material_offset, repository);
      surfaces.push_back(surface);
    }
    return surfaces;
  }

  void GltfParser::create_nodes(fastgltf::Asset& gltf, const size_t& mesh_offset,
      ComponentFactory* component_factory) {
    for (fastgltf::Node& node : gltf.nodes) {
      glm::vec3 position(0.f);
      glm::quat orientation(1.f, 0.f, 0.f, 0.f);
      float uniform_scale = 1.f;

      if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
        const auto& [translation, rotation, scale] = std::get<fastgltf::TRS>(node.transform);

        position = glm::vec3(translation[0], translation[1], translation[2]);
        orientation = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);
        uniform_scale = (scale[0] + scale[1] + scale[2]) / 3.f; // handle non-uniform scale
      }

      const auto [entity, node_component] = component_factory->create_entity(
          std::string(node.name), position, orientation, uniform_scale);

      if (node.lightIndex.has_value()) {
        auto light = gltf.lights.at(node.lightIndex.value());

        if (light.type == fastgltf::LightType::Directional) {
          glm::vec3 direction = compute_direction(orientation);
          component_factory->create_directional_light(
              glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)), light.intensity,
              direction, entity);

        } else if (light.type == fastgltf::LightType::Point) {
          float32 range = light.range.value_or(-1.f);  // -1 means infinite range

          component_factory->create_point_light(
              glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)), light.intensity,
              position, range, entity);

        } else if (light.type == fastgltf::LightType::Spot) {
          float32 innerConeRadians = light.innerConeAngle.value_or(0.f);
          float32 outerConeRadians = light.outerConeAngle.value_or(3.141f / 4.f);

          innerConeRadians = std::max<float32>(innerConeRadians, 0);

          if (outerConeRadians < innerConeRadians ) {
            fmt::print(
                "Outer cone angle is less than inner cone angle\n");
          }

          component_factory->create_spot_light(
              glm::vec3(light.color.at(0), light.color.at(1), light.color.at(2)), light.intensity,
              compute_direction(orientation), position, light.range.value_or(-1.f),
              innerConeRadians, outerConeRadians,
              entity);
        }
      }

      if (node.cameraIndex.has_value()) {
        fastgltf::Camera cam = gltf.cameras.at(node.cameraIndex.value());
        if (std::holds_alternative<fastgltf::Camera::Perspective>(cam.camera)) {
          const auto& perspective = std::get<fastgltf::Camera::Perspective>(cam.camera);
          const float32 aspect_ratio = perspective.aspectRatio.value_or(1.f);
          const float32 fov = perspective.yfov; // in radians
          const float32 near = perspective.znear;
          const float32 far = perspective.zfar.value_or(1000.f);
          component_factory->add_animation_camera(
              position, orientation, entity,
              PerspectiveProjectionComponent{fov, near, far, aspect_ratio});
        } else if (std::holds_alternative<fastgltf::Camera::Orthographic>(cam.camera)) {
          const auto& ortho = std::get<fastgltf::Camera::Orthographic>(cam.camera);
          const float32 xmag = ortho.xmag;
          const float32 ymag = ortho.ymag;
          const float32 near = ortho.znear;
          const float32 far = ortho.zfar;
          component_factory->add_animation_camera(
              position, orientation, entity,
              OrthographicProjectionComponent{xmag, ymag, near, far});
        }
      }

      if (node.meshIndex.has_value()) {
        component_factory->add_mesh_component(entity, mesh_offset + *node.meshIndex);
      }
    }
  }

  void GltfParser::build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset,
      Repository* repository) {
    for (int i = 0; i < nodes.size(); i++) {
      fastgltf::Node& node = nodes[i];
      Entity parent_entity = static_cast<Entity>(node_offset + i);
      auto scene_object = repository->scene_graph.find_mutable(parent_entity);

      for (auto& c : node.children) {
        Entity child_entity = static_cast<Entity>(node_offset + c);
        auto child = repository->scene_graph.find_mutable(child_entity);
        scene_object->children.push_back(child_entity);
        child->parent = parent_entity;
      }
    }
  }

  void GltfParser::link_orphans_to_root(Entity root, NodeComponent* root_node,
                                        const std::vector<std::pair<Entity, NodeComponent>>& nodes, Repository* repository) {
    for (auto& [entity, node] : nodes) {
      if (entity == root) {
        continue;
      }

      if (node.parent == invalid_entity) {
        root_node->children.push_back(entity);
        repository->scene_graph.find_mutable(entity)->parent = root;
      }
    }
  }
}  // namespace gestalt::application