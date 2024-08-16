#pragma once

#include "common.hpp"
#include <glm/gtx/euler_angles.hpp>

namespace gestalt::foundation {

  struct AnimationCameraData {

    AnimationCameraData(const glm::vec3& pos, const glm::vec3& angles, const glm::vec3& desired_pos, const glm::vec3& desired_angles)
        : position_current(pos),
          position_desired(desired_pos),
          angles_current(angles),
          angles_desired(desired_angles) {}

    [[nodiscard]] glm::mat4 get_view_matrix() const {
      const glm::vec3 a = radians(angles_current);
      return translate(glm::yawPitchRoll(a.y, a.x, a.z), -position_current);
    }

    // Configuration Values
    glm::vec3 position_current = glm::vec3(0.0f);
    glm::vec3 position_desired = glm::vec3(0.0f);
    /// pitch, pan, roll
    glm::vec3 angles_current = glm::vec3(0.0f);
    glm::vec3 angles_desired = glm::vec3(0.0f);

    // Adjustable Parameters
    float32 damping_linear = 10000.0f;
    glm::vec3 damping_euler_angles = glm::vec3(5.0f, 5.0f, 5.0f);
  };
}  // namespace gestalt