
#include "EntityManager.hpp"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>

#include <glm/gtx/matrix_decompose.hpp>

#include "Components/PhysicsComponent.hpp"
#include "Mesh/MeshSurface.hpp"
#include "fmt/printf.h"

namespace gestalt::application {

    Entity ComponentFactory::next_entity() { return next_entity_id_++; }

    std::pair<Entity, std::reference_wrapper<NodeComponent>> ComponentFactory::create_entity(
        std::string node_name, const glm::vec3& position, const glm::quat& rotation,
        const float& scale) {
      const Entity new_entity = next_entity();

      if (node_name.empty()) {
        node_name = "entity_" + std::to_string(new_entity);
      }
      const NodeComponent node = {
          .name = node_name,
      };

      create_transform_component(new_entity, position, rotation, scale);
      repository_->scene_graph.add(new_entity, node);

      fmt::print("created entity {}\n", new_entity);

      return std::make_pair(new_entity, std::ref(repository_->scene_graph.get(new_entity)->get()));
    }

    void ComponentFactory::create_transform_component(const Entity entity,
                                                               const glm::vec3& position,
                                                               const glm::quat& rotation,
                                                               const float& scale) const {
      repository_->transform_components.add(entity, TransformComponent{true, position, rotation, scale});
    }

    void ComponentFactory::add_mesh_component(const Entity entity,
                                                       const size_t mesh_index) {
      assert(entity != invalid_entity);

      repository_->mesh_components.add(entity, MeshComponent{{true}, mesh_index});
    }

  void ComponentFactory::create_mesh(std::vector<MeshSurface> surfaces, const std::string& name) const {
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

      glm::vec3 min(FLT_MAX);
      glm::vec3 max(-FLT_MAX);

      for (const auto& surface : surfaces) {
        min = glm::min(min, surface.local_aabb.min);
        max = glm::max(max, surface.local_aabb.max);
      }

      mesh_id = repository_->meshes.add(
          Mesh{key, std::move(surfaces), BoundingSphere{combined_center, combined_radius}, AABB{min, max}});
      fmt::print("created mesh {}, mesh_id {}\n", key, mesh_id);
    }

  void ComponentFactory::create_physics_component(const Entity entity, const BodyType body_type,
                                                  const BoxCollider& collider) const {
      repository_->physics_components.add(entity, PhysicsComponent(body_type, collider));
    }

  void ComponentFactory::create_physics_component(const Entity entity, const BodyType body_type,
                                                  const SphereCollider& collider) const {
      repository_->physics_components.add(entity, PhysicsComponent(body_type, collider));
    }

    Entity ComponentFactory::create_directional_light(const glm::vec3& color,
                                                               const float intensity,
                                                               const glm::vec3& direction,
                                                               Entity parent) {
      auto [entity, node] = create_entity("directional_light");
      link_entity_to_parent(entity, parent);

      auto& transform = repository_->transform_components.get(entity).value().get();
      transform.rotation = glm::quat(lookAt(glm::vec3(0), direction, glm::vec3(0, 1, 0)));

      const uint32 matrix_id = repository_->light_view_projections.add(glm::mat4(1.0));
      const LightComponent light(color, intensity, matrix_id);

      repository_->light_components.add(entity, light);

      return entity;
    }

    Entity ComponentFactory::create_spot_light(
        const glm::vec3& color, const float intensity, const glm::vec3& direction,
        const glm::vec3& position, const float innerCone, const float outerCone, Entity parent) {
      auto [entity, node] = create_entity("spot_light");
      link_entity_to_parent(entity, parent);

      auto& transform = repository_->transform_components.get(entity).value().get();
      transform.rotation = glm::quat(lookAt(glm::vec3(0), direction, glm::vec3(0, 1, 0)));
      transform.position = position;

      const LightComponent light(color, intensity, innerCone, outerCone);

      repository_->light_components.add(entity, light);

      return entity;
    }

    Entity ComponentFactory::create_point_light(const glm::vec3& color,
                                                         const float32 intensity,
                                                         const glm::vec3& position, const float32 range, Entity parent) {
      auto [entity, node] = create_entity("point_light");
      link_entity_to_parent(entity, parent);

      auto& transform = repository_->transform_components.get(entity).value().get();
      transform.position = position;

      const uint32 matrix_id = repository_->light_view_projections.add(glm::mat4(1.0));
      for (int i = 0; i < 5; i++) {
        repository_->light_view_projections.add(glm::mat4(1.0));
      }
      const LightComponent light(color, intensity, range, matrix_id);

      repository_->light_components.add(entity, light);
      return entity;
    }

  Entity ComponentFactory::add_free_fly_camera(const glm::vec3& position,
                                                    const glm::vec3& direction, const glm::vec3& up, Entity entity) const {
      const auto free_fly_component = CameraComponent(
          kPerspective,
          FreeFlyCameraData(position, direction, up));
      repository_->camera_components.add(entity, free_fly_component);

      return entity;
    }

  Entity ComponentFactory::add_orbit_camera(const glm::vec3& target, Entity entity) const {
      const auto orbit_component = CameraComponent(
          kPerspective,
          OrbitCameraData(target));
      repository_->camera_components.add(entity, orbit_component);

      return entity;
    }

  Entity ComponentFactory::add_first_person_camera(const glm::vec3& position, Entity entity) const {
      const auto first_person_component = CameraComponent(
          kPerspective,
          FirstPersonCameraData(position, glm::vec3(0.f,1.f, 0.f)));
      repository_->camera_components.add(entity, first_person_component);

      return entity;
    }

    void ComponentFactory::link_entity_to_parent(const Entity child, const Entity parent) {
      if (child == parent) {
        return;
      }

      const auto& parent_node = repository_->scene_graph.get(parent);
      const auto& child_node = repository_->scene_graph.get(child);

      if (parent_node.has_value() && child_node.has_value()) {
        parent_node->get().children.push_back(child);
        child_node->get().parent = parent;
      }
    }

    void ComponentFactory::init(IResourceManager* resource_manager,
                                         Repository* repository) {
      resource_manager_ = resource_manager;
      repository_ = repository;

      auto [entity, root_node] = create_entity();
      root_node.get().name = "root";
    }

}  // namespace gestalt