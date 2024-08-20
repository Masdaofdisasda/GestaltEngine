#include "SceneSystem.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <glm/gtx/string_cast.hpp>

#include "Components/PhysicsComponent.hpp"
#include "Jolt/Physics/Body/Body.h"
#include "fmt/core.h"


namespace gestalt::application {
  void PhysicSystem::prepare() {
    physic_engine_ = std::make_unique<PhysicEngine>();
    physic_engine_->init();
  }

  void PhysicSystem::update(const float delta_time) const {
    physic_engine_->step_simulation(delta_time);

    for (auto& [entity, physics_component] : repository_->physics_components.components()) {

      if (physics_component.body == nullptr) {
        physics_component.body = physic_engine_->create_body(
            physics_component, repository_->transform_components[entity].position,
            repository_->transform_components[entity].rotation);
      }

      if (physics_component.body_type == DYNAMIC) {
        glm::vec3 position;
        glm::quat orientation;
        physic_engine_->get_body_transform(physics_component.body, position,
                                           orientation);
        repository_->transform_components[entity].position = position;
        repository_->transform_components[entity].rotation = orientation;
        fmt::print("Entity: {}, Position: {}, Orientation: {}\n", entity, to_string(position),
                   to_string(orientation));
      }
    }
  }

  void PhysicSystem::update() { assert(!"Not supported"); }

  void PhysicSystem::cleanup() {}

}  // namespace gestalt::application