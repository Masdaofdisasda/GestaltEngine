#pragma once

#include "common.hpp"
#include <glm/gtx/quaternion.hpp>

namespace gestalt::foundation {

  struct FirstPersonCameraData {

    FirstPersonCameraData(const glm::vec3& start_position, const glm::vec3& start_up) {
      set_position(start_position);
      set_orientation(start_position + glm::vec3(0.0f, 0.0f, -1.0f), start_up);
      set_up_vector(start_up);
    }

    void set_position(const glm::vec3& pos) {
      position = pos;
    }

    void set_orientation(const glm::vec3& target, const glm::vec3& up) {
      orientation = quat_cast(lookAtRH(position, target, up));
    }

    void set_up_vector(const glm::vec3& up) {
      this->up = up;
      const glm::mat4 view = get_view_matrix();
      const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
      orientation = lookAtRH(position, position + dir, up);
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
      const glm::mat4 t = translate(glm::mat4(1.0f), -position);
      const glm::mat4 r = mat4_cast(orientation);
      return r * t;
    }

    // Configuration Values
    glm::vec3 position;
    glm::quat orientation = glm::quat(glm::vec3(0.0f));
    glm::vec3 up;

    // Adjustable Parameters
    float32 mouse_speed = 2.5f;  // Adjust based on your desired sensitivity

    // State Variables
    glm::vec2 mouse_pos = glm::vec2(0.0f);
  };

}  // namespace gestalt