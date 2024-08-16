#pragma once
#include "Component.hpp"
#include "common.hpp"

namespace gestalt::foundation {

    struct TransformComponent : Component {
  glm::vec3 position;
  glm::quat rotation;
  float32 scale;  // uniform scale for now

  glm::vec3 parent_position;
  glm::quat parent_rotation;
  float32 parent_scale;

  TransformComponent operator*(const TransformComponent& local_transform) const {
    TransformComponent result;

    // Compute the combined world position
    result.position = position + rotation * (scale * local_transform.position);

    // Compute the combined world rotation
    result.rotation = rotation * local_transform.rotation;

    // Compute the combined world scale
    result.scale = scale * local_transform.scale;

    return result;
  }
};

}  // namespace gestalt