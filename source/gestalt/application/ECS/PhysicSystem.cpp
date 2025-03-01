#include "PhysicSystem.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <fmt/core.h>

#include <glm/gtx/string_cast.hpp>

#include "PhysicUtil.hpp"
#include "UserInput.hpp"
#include "Components/PhysicsComponent.hpp"
#include "Events/EventBus.hpp"
#include "Events/Events.hpp"


namespace gestalt::application {

  
  PhysicSystem::PhysicSystem(IGpu& gpu, IResourceAllocator& resource_allocator,
                             Repository& repository, FrameProvider& frame, EventBus& event_bus)
      : gpu_(gpu),
        resource_allocator_(resource_allocator),
        repository_(repository),
        frame_(frame),
  event_bus_(event_bus)
  {
    physic_engine_ = std::make_unique<PhysicEngine>();
    physic_engine_->init();

    for (auto& [entity, physics_component] : repository_.physics_components.snapshot()) {
      if (physics_component.collider_type == CAPSULE) {
        player_ = entity;
      }
    }
  }

  void PhysicSystem::move_player(const float delta_time, const UserInput& movement) const {
    const auto player_physics = repository_.physics_components.find(player_);
    if (player_physics->body == nullptr) return;
    const auto player_transform = repository_.transform_components.find(player_);

    auto orientation = player_transform->rotation();
    orientation.w *= -1; // Jolt uses a different handedness convention (i guess)
    glm::vec3 forward = orientation * glm::vec3(0, 0, -1);
    glm::vec3 right = orientation * glm::vec3(1, 0, 0);
    forward.y = 0; // ignore vertical movement
    right.y = 0;
    forward = glm::normalize(forward); // ensure consistent speed in all directions
    right = glm::normalize(right);

    const JPH::Vec3 jph_forward = to(forward);
    const JPH::Vec3 jph_right = to(right);

    // Calculate movement direction based on input
    JPH::Vec3 movement_direction(0.0f, 0.0f, 0.0f);
    constexpr float movement_speed = 250000.0f;

    if (movement.forward) movement_direction += jph_forward;
    if (movement.backward) movement_direction -= jph_forward;
    if (movement.left) movement_direction -= jph_right;
    if (movement.right) movement_direction += jph_right;

    if (movement_direction.LengthSq() > 0.0f) {  // Avoid division by zero
      movement_direction = movement_direction.Normalized();
    }

    player_physics->body->AddForce(movement_direction * movement_speed * delta_time);

    // Ground detection logic (simplified for illustration)
    bool mIsGrounded = true;  // check_if_grounded(player_physics.body);
    JPH::Vec3 jump_strength = JPH::Vec3(0, 20.f * movement_speed * delta_time, 0);

    // Apply jump if grounded
    if (movement.up && mIsGrounded) {
       player_physics->body->AddForce(jump_strength);
    }
  }

  void PhysicSystem::update(float delta_time, const UserInput& movement) const {

    move_player(delta_time, movement);

    physic_engine_->step_simulation(delta_time);

    for (auto& [entity, physics_component] : repository_.physics_components.snapshot()) {

      if (physics_component.body == nullptr) {
        auto transform = repository_.transform_components.find(entity);
        physics_component.body = physic_engine_->create_body(
            physics_component, transform->position(), transform->rotation());
      }

      if (physics_component.body_type == DYNAMIC) {
        glm::vec3 position;
        glm::quat orientation;
        physic_engine_->get_body_transform(physics_component.body, position,
                                           orientation);
        event_bus_.emit<TranslateEntityEvent>({entity, position});
        event_bus_.emit<RotateEntityEvent>({entity, orientation});
      }
    }
  }

}  // namespace gestalt::application