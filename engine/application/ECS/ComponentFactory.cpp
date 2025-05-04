
#include "ComponentFactory.hpp"

#include <fmt/core.h>

#include "Repository.hpp"
#include "Animation/Keyframe.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/DirectionalLightComponent.hpp"
#include "Components/MeshComponent.hpp"
#include "Components/NodeComponent.hpp"
#include "Components/PhysicsComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/SpotLightComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Events/EventBus.hpp"
#include "Events/Events.hpp"
#include "Mesh/Mesh.hpp"
#include "Mesh/MeshSurface.hpp"

static glm::quat orientation_from_direction(const glm::vec3& direction, const glm::vec3& base_forward = glm::vec3(0.f,0.f,-1.f)) {

  glm::vec3 dir = glm::normalize(direction);
  glm::vec3 base = glm::normalize(base_forward);

  // clamp to avoid numerical issues in acos
  float dotVal = glm::clamp(glm::dot(base, dir), -1.0f, 1.0f);
  float angle = acosf(dotVal);

  // If base and dir are almost the same, angle ~ 0 => use identity quaternion
  // If base and dir are nearly opposite, angle ~ pi => cross might be zero if they are collinear

  glm::vec3 axis = glm::cross(base, dir);

  // If cross is close to zero length, pick any orthonormal axis, e.g. (1,0,0)
  float lengthSq = glm::dot(axis, axis);
  if (lengthSq < 1e-8f) {
    // They are parallel or nearly so. Check if they are in the same or opposite direction
    if (dotVal > 0.0f) {
      // same direction => no rotation
      return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    } else {
      // opposite => rotate 180° about some axis perpendicular to base
      // e.g. pick an axis perpendicular to base
      axis = glm::vec3(0.f, 1.f, 0.f);
      angle = 3.1415926535f;  // pi
    }
  } else {
    // 4) normalize axis
    axis = glm::normalize(axis);
  }

  // 5) Build a quaternion from axis-angle
  // q = cos(a/2) + sin(a/2)*(axis.x, axis.y, axis.z)
  float halfAngle = 0.5f * angle;
  float s = sinf(halfAngle);

  glm::quat q;
  q.w = cosf(halfAngle);
  q.x = axis.x * s;
  q.y = axis.y * s;
  q.z = axis.z * s;

  return glm::normalize(q);
}

namespace gestalt::application {

  ComponentFactory::ComponentFactory(Repository& repository, EventBus& event_bus)
      : repository_(repository), event_bus_(event_bus) {
    auto [entity, root_node] = create_entity();
    root_node->name = "root";
  }

    Entity ComponentFactory::next_entity() { return next_entity_id_++; }

    std::pair<Entity, NodeComponent*> ComponentFactory::create_entity(std::string node_name,
                                                                      const glm::vec3& position,
                                                                      const glm::quat& rotation,
                                                                      const float& scale) {
      const Entity new_entity = next_entity();

      if (node_name.empty()) {
        node_name = "entity_" + std::to_string(new_entity);
      }
      const NodeComponent node = {
          .name = node_name,
      };

      create_transform_component(new_entity, position, rotation, scale);
      repository_.scene_graph.upsert(new_entity, node);

      fmt::print("created entity {}\n", new_entity);

      return std::make_pair(new_entity, repository_.scene_graph.find_mutable(new_entity));
    }

    void ComponentFactory::create_transform_component(const Entity entity,
                                                               const glm::vec3& position,
                                                               const glm::quat& rotation,
                                                               const float& scale) const {
      const auto transform = TransformComponent(position, rotation, scale);
      transform.is_dirty = true;
      repository_.transform_components.upsert(entity, transform);
    }

    void ComponentFactory::add_mesh_component(const Entity entity,
                                              const size_t mesh_index) {
      assert(entity != invalid_entity);

      repository_.mesh_components.upsert(entity, MeshComponent{{true}, mesh_index});
    }

  void ComponentFactory::create_mesh(std::vector<MeshSurface> surfaces, const std::string& name) const {
      size_t mesh_id = repository_.meshes.size();
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

      mesh_id = repository_.meshes.add(
          Mesh{key, std::move(surfaces), BoundingSphere{combined_center, combined_radius}, AABB{min, max}});
      fmt::print("created mesh {}, mesh_id {}\n", key, mesh_id);
    }

  void ComponentFactory::create_physics_component(const Entity entity, const BodyType body_type,
                                                  const BoxCollider& collider) const {
      repository_.physics_components.upsert(entity, PhysicsComponent(body_type, collider));
    }

  void ComponentFactory::create_physics_component(const Entity entity, const BodyType body_type,
                                                  const SphereCollider& collider) const {
      repository_.physics_components.upsert(entity, PhysicsComponent(body_type, collider));
    }

  void ComponentFactory::create_physics_component(const Entity entity, const BodyType body_type,
                                                  const CapsuleCollider& collider) const {
      repository_.physics_components.upsert(entity, PhysicsComponent(body_type, collider));
    }

  void ComponentFactory::create_animation_component(
        const Entity entity, const std::vector<Keyframe<glm::vec3>>& translation_keyframes,
        const std::vector<Keyframe<glm::quat>>& rotation_keyframes,
        const std::vector<Keyframe<glm::vec3>>& scale_keyframes) const {
    repository_.animation_components.upsert(
        entity, AnimationComponent(translation_keyframes, rotation_keyframes, scale_keyframes));
    }

    Entity ComponentFactory::create_directional_light(const glm::vec3& color,
                                                               const float intensity,
                                                               const glm::vec3& direction,
                                                      Entity entity) {
      if (entity == invalid_entity) {
        const auto number_of_lights = repository_.directional_light_components.size();
        auto [new_entity, node]
            = create_entity("directional_light_" + std::to_string(number_of_lights + 1));
        entity = new_entity;
        link_entity_to_parent(entity, root_entity);
        auto transform = repository_.transform_components.find(entity);
        event_bus_.emit<MoveEntityEvent>(
            MoveEntityEvent{entity, transform->position(), orientation_from_direction(direction),transform->scale_uniform()
        });
      }


      const uint32 matrix_id
          = repository_.light_view_projections.add({glm::mat4(1.0), glm::mat4(1.0)});
      const DirectionalLightComponent light(color, intensity, matrix_id);

      repository_.directional_light_components.upsert(entity, light);

      return entity;
    }

    Entity ComponentFactory::create_spot_light(
        const glm::vec3& color, const float32 intensity, const glm::vec3& direction,
                                                const glm::vec3& position, const float32 range,
                                                const float32 inner_cone_radians,
                                                const float32 outer_cone_radians, Entity entity) {
      if (entity == invalid_entity) {
        const auto number_of_lights = repository_.spot_light_components.size();
        auto [new_entity, node]
            = create_entity("spot_light_" + std::to_string(number_of_lights + 1));
        entity = new_entity;
          link_entity_to_parent(entity, root_entity);
        event_bus_.emit<MoveEntityEvent>(
            MoveEntityEvent{entity, position,orientation_from_direction(direction),1.f});
      }
      const uint32 matrix_id
          = repository_.light_view_projections.add({glm::mat4(1.0), glm::mat4(1.0)});

      const SpotLightComponent light(color, intensity, range, matrix_id, cos(inner_cone_radians), cos(outer_cone_radians));

      repository_.spot_light_components.upsert(entity, light);

      return entity;
    }

    Entity ComponentFactory::create_point_light(const glm::vec3& color,
                                                         const float32 intensity,
                                                         const glm::vec3& position, const float32 range, Entity entity) {
      if (entity == invalid_entity) {
        const auto number_of_lights = repository_.point_light_components.size();
        auto [new_entity, node]
            = create_entity("point_light_" + std::to_string(number_of_lights + 1));
        entity = new_entity;
          link_entity_to_parent(entity, root_entity);
        auto transform = repository_.transform_components.find(entity);
          event_bus_.emit<MoveEntityEvent>(
              MoveEntityEvent{entity,position,transform->rotation(), transform->scale_uniform()
        });
      }


      const uint32 matrix_id
          = repository_.light_view_projections.add({glm::mat4(1.0), glm::mat4(1.0)});
      for (int i = 0; i < 5; i++) {
        repository_.light_view_projections.add({glm::mat4(1.0), glm::mat4(1.0)});
      }
      const PointLightComponent light(color, intensity, range, matrix_id);

      repository_.point_light_components.upsert(entity, light);
      return entity;
    }

  Entity ComponentFactory::add_free_fly_camera(const glm::vec3& position,
                                                 const glm::vec3& direction, const glm::vec3& up,
                                                 const Entity entity,
                                                 const PerspectiveProjectionComponent projection) const {
      const auto free_fly_component
        = FreeFlyCameraComponent(position, direction, up);
      repository_.free_fly_camera_components.upsert(entity, free_fly_component);
      repository_.perspective_projection_components.upsert(entity, projection);

      return entity;
  }

  Entity ComponentFactory::add_free_fly_camera(const glm::vec3& position,
                                                 const glm::vec3& direction, const glm::vec3& up,
                                                 const Entity entity,
                                                 const OrthographicProjectionComponent projection) const {
      const auto free_fly_component
        = FreeFlyCameraComponent(position, direction, up);
      repository_.free_fly_camera_components.upsert(entity, free_fly_component);
      repository_.orthographic_projection_components.upsert(entity, projection);

      return entity;
  }

  Entity ComponentFactory::add_animation_camera(const glm::vec3& position, const glm::quat& orientation, const Entity entity,
                                                const PerspectiveProjectionComponent projection) const {
    const auto animation_component
        = AnimationCameraComponent(position, orientation);
    repository_.animation_camera_components.upsert(entity, animation_component);
    repository_.perspective_projection_components.upsert(entity, projection);

    return entity;
  }

  Entity ComponentFactory::add_animation_camera(const glm::vec3& position, const glm::quat& orientation, const Entity entity,
                                                const OrthographicProjectionComponent projection) const {
    const auto animation_component
        = AnimationCameraComponent(position, orientation);
    repository_.animation_camera_components.upsert(entity, animation_component);
    repository_.orthographic_projection_components.upsert(entity, projection);

    return entity;
  }

  Entity ComponentFactory::add_orbit_camera(const glm::vec3& target, const Entity entity,
                                            const PerspectiveProjectionComponent projection) const {
    const auto orbit_component = OrbitCameraComponent(target);
    repository_.orbit_camera_components.upsert(entity, orbit_component);
    repository_.perspective_projection_components.upsert(entity, projection);

    return entity;
  }
  Entity ComponentFactory::add_orbit_camera(const glm::vec3& target, const Entity entity,
                                            const OrthographicProjectionComponent projection) const {
    const auto orbit_component = OrbitCameraComponent(target);
    repository_.orbit_camera_components.upsert(entity, orbit_component);
    repository_.orthographic_projection_components.upsert(entity, projection);

    return entity;
  }

  Entity ComponentFactory::add_first_person_camera(
      const glm::vec3& position, const Entity entity,
      const PerspectiveProjectionComponent projection) const {
    const auto first_person_component
        = FirstPersonCameraComponent(position, glm::vec3(0.f, 1.f, 0.f));
    repository_.first_person_camera_components.upsert(entity, first_person_component);
    repository_.perspective_projection_components.upsert(entity, projection);

    return entity;
  }

  Entity ComponentFactory::add_first_person_camera(
      const glm::vec3& position, const Entity entity,
      const OrthographicProjectionComponent projection) const {
    const auto first_person_component
        = FirstPersonCameraComponent(position, glm::vec3(0.f, 1.f, 0.f));
    repository_.first_person_camera_components.upsert(entity, first_person_component);
    repository_.orthographic_projection_components.upsert(entity, projection);

    return entity;
  }

    void ComponentFactory::link_entity_to_parent(const Entity child, const Entity parent) {
      if (child == parent) {
        return;
      }

      const auto parent_node = repository_.scene_graph.find_mutable(parent);
      const auto child_node = repository_.scene_graph.find_mutable(child);

      if (parent_node != nullptr && child_node != nullptr) {
        parent_node->children.push_back(child);
        child_node->parent = parent;
      }
    }

}  // namespace gestalt