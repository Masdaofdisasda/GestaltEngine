#pragma once

#include <string>
#include <vector>
#include <glm/fwd.hpp>

#include "common.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/Entity.hpp"
#include <Animation/Keyframe.hpp>

#include "Components/PhysicsComponent.hpp"

namespace gestalt::foundation {
  struct MeshSurface;
  struct NodeComponent;
  class Repository;
}

namespace gestalt::application {

    constexpr float32 kDefaultFov = 1.22173f;  // 70 degrees

    class ComponentFactory {
      Repository& repository_;

      Entity next_entity_id_{0};

      Entity next_entity();
      void create_transform_component(unsigned entity, const glm::vec3& position = glm::vec3(0.f),
                                      const glm::quat& rotation = glm::quat(1.f, 0.f, 0.f, 0.f),
                                      const float& scale = 1.f) const;

    public:
      explicit ComponentFactory(Repository& repository);
      ~ComponentFactory() = default;

      ComponentFactory(const ComponentFactory&) = delete;
      ComponentFactory& operator=(const ComponentFactory&) = delete;

      ComponentFactory(ComponentFactory&&) = delete;
      ComponentFactory& operator=(ComponentFactory&&) = delete;

      std::pair<Entity, std::reference_wrapper<NodeComponent>> create_entity(
          std::string node_name = "", const glm::vec3& position = glm::vec3(0.f),
          const glm::quat& rotation = glm::quat(1.f, 0.f, 0.f, 0.f), const float& scale = 1.f);
      void add_mesh_component(Entity entity, size_t mesh_index);
      void create_mesh(std::vector<MeshSurface> surfaces, const std::string& name) const;
      void create_physics_component(Entity entity, BodyType body_type,
                                    const BoxCollider& collider) const;
      void create_physics_component(Entity entity, BodyType body_type,
                                    const SphereCollider& collider) const;
      void create_physics_component(Entity entity, BodyType body_type,
                                    const CapsuleCollider& collider) const;

      void create_animation_component(const Entity entity,
                                      const std::vector<Keyframe<glm::vec3>>& translation_keyframes,
                                      const std::vector<Keyframe<glm::quat>>& rotation_keyframes,
                                      const std::vector<Keyframe<glm::vec3>>& scale_keyframes) const;

      Entity create_directional_light(const glm::vec3& color, float intensity,
                                      const glm::vec3& direction,
                                      const Entity entity = invalid_entity);
      Entity create_spot_light(const glm::vec3& color, float32 intensity,
                               const glm::vec3& direction, const glm::vec3& position, float32 range,
                               float32 inner_cone_radians, float32 outer_cone_radians,
                               Entity entity = invalid_entity);
      Entity create_point_light(const glm::vec3& color, float intensity, const glm::vec3& position,
                                float32 range, const Entity entity = invalid_entity);
      Entity add_free_fly_camera(const glm::vec3& position, const glm::vec3& direction,
                                 const glm::vec3& up, Entity entity,
                                 ProjectionData projection_data
                                 = PerspectiveProjectionData(kDefaultFov, 1.0f, 0.1f,
                                                             1000.f)) const;

      Entity add_animation_camera(const glm::vec3& position, const glm::quat& orientation,
                                  Entity entity,
                                  ProjectionData projection_data
                                  = PerspectiveProjectionData(kDefaultFov, 1.0f, 0.1f,
                                                              1000.f)) const;

      Entity add_orbit_camera(const glm::vec3& target, Entity entity,
                              ProjectionData projection_data
                              = PerspectiveProjectionData(kDefaultFov, 1.0f, 0.1f, 1000.f)) const;

      Entity add_first_person_camera(const glm::vec3& position, Entity entity,
                                     ProjectionData projection_data
                                     = PerspectiveProjectionData(kDefaultFov, 1.0f, 0.1f,
                                                                 1000.f)) const;

      void link_entity_to_parent(Entity child, Entity parent);
    };

}  // namespace gestalt::application