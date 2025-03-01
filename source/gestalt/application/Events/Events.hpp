#pragma once
#include "Components/Entity.hpp"
#include "glm/fwd.hpp"

namespace gestalt::application {
  struct MoveEntityEvent {
    MoveEntityEvent(Entity entity, glm::vec3 new_position, glm::quat new_rotation, float32 new_scale)
        : entity(entity), new_position(new_position), new_rotation(new_rotation), new_scale(new_scale) {}
    Entity entity;
    glm::vec3 new_position;
    glm::quat new_rotation;
    glm::vec3 new_scale;
  };
  struct TranslateEntityEvent {
    TranslateEntityEvent(Entity entity, glm::vec3 new_position)
        : entity(entity), new_position(new_position) {}
    Entity entity;
    glm::vec3 new_position;
  };
  struct RotateEntityEvent {
    RotateEntityEvent(Entity entity, glm::quat new_rotation)
        : entity(entity), new_rotation(new_rotation) {}
    Entity entity;
    glm::quat new_rotation;
  };
  struct ScaleEntityEvent {
    ScaleEntityEvent(Entity entity, float32 new_scale) : entity(entity), new_scale(new_scale) {}
    Entity entity;
    float32 new_scale;
  };
}  // namespace gestalt::application