﻿#pragma once

#include "common.hpp"
#include <glm/gtx/euler_angles.hpp>

namespace gestalt::foundation {

  struct AnimationCameraData {

    AnimationCameraData(const glm::vec3& starting_pos, const glm::quat& starting_orientation)
        : position(starting_pos), orientation(starting_orientation) {}

    [[nodiscard]] glm::mat4 get_view_matrix() const {
      glm::mat4 worldTransform
          = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(orientation);
      return glm::inverse(worldTransform);
    }

    void set_position(const glm::vec3& new_position) { position = new_position; }
    void set_orientation(const glm::quat& new_orientation) { orientation = new_orientation; }
    void set_euler_angles(float pitch, float yaw, float roll) {
      orientation = glm::quat(glm::vec3(yaw, pitch, roll));
    }

    // Configuration Values
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  };
}  // namespace gestalt