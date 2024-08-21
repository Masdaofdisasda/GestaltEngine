#include "SceneSystem.hpp"

#include <Jolt/Jolt.h>

#include <glm/gtx/string_cast.hpp>

#include "PhysicUtil.hpp"
#include "UserInput.hpp"
#include "Components/PhysicsComponent.hpp"
#include "Jolt/Physics/Body/Body.h"
#include "fmt/core.h"


namespace gestalt::application {
  void PhysicSystem::prepare() {
    physic_engine_ = std::make_unique<PhysicEngine>();
    physic_engine_->init();

    for (auto& [entity, physics_component] : repository_->physics_components.components()) {
      if (physics_component.collider_type == CAPSULE) {
        player_ = entity;
      }
    }
  }

  void PhysicSystem::move_player(const float delta_time, const UserInput& movement) const {
    auto& player_physics = repository_->physics_components[player_];
    if (player_physics.body == nullptr) return;
    auto& player_transform = repository_->transform_components[player_];

    // Transform movement direction based on the player's orientation
    glm::vec3 forward = player_transform.rotation * glm::vec3(0, 0, -1);  // Forward
    glm::vec3 right = player_transform.rotation * glm::vec3(1, 0, 0);     // Right

    // Calculate movement direction based on input
    JPH::Vec3 movement_force(0.0f, 0.0f, 0.0f);
    constexpr float movement_speed = 750000.0f;


    // Calculate movement direction based on input
    if (movement.forward) movement_force += to(forward);
    if (movement.backward) movement_force -= to(forward);
    if (movement.left) movement_force -= to(right);
    if (movement.right) movement_force += to(right);

    player_physics.body->AddForce(movement_force * movement_speed * delta_time);

    // Ground detection logic (simplified for illustration)
    bool mIsGrounded = true;  // check_if_grounded(player_physics.body);
    JPH::Vec3 jump_strength = JPH::Vec3(0, 20.f * movement_speed * delta_time, 0);

    // Apply jump if grounded
    if (movement.up && mIsGrounded) {
       player_physics.body->AddForce(jump_strength);
    }
  }


  void PhysicSystem::update(const float delta_time, const UserInput& movement) const {

    move_player(delta_time, movement);

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
      }
    }
  }

  void PhysicSystem::update() { assert(!"Not supported"); }

  void PhysicSystem::cleanup() {}

}  // namespace gestalt::application