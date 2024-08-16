#pragma once

#include "common.hpp"
#include <glm/gtx/quaternion.hpp>

namespace gestalt::foundation {

  struct FreeFlyCameraData {

    FreeFlyCameraData(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) {
      set_position(pos);
      set_orientation(target, up);
      set_up_vector(up);
    }

    void set_up_vector(const glm::vec3& up) {
      this->up = up;
      const glm::mat4 view = get_view_matrix();
      const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
      camera_orientation = lookAtRH(camera_position, camera_position + dir, up);
    }

    void set_position(const glm::vec3& pos) {
      camera_position = pos;
    }

    void set_orientation(const glm::vec3& target, const glm::vec3& up) {
      camera_orientation = quat_cast(lookAtRH(camera_position, target, up));
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
      const glm::mat4 t = translate(glm::mat4(1.0f), -camera_position);
      const glm::mat4 r = mat4_cast(camera_orientation);
      return r * t;
    }

    void reset_mouse_position(const glm::vec2& p) { mouse_pos = p; }

    glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 1.0f);
    glm::quat camera_orientation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
    glm::vec3 move_speed = glm::vec3(0.0f);
    glm::vec2 mouse_pos = glm::vec2(0);

    float32 mouse_speed = 4.5f;
    float32 acceleration = .01f;
    float32 damping = 15.f;
    float32 max_speed = .05f;
    float32 fast_coef = 5.f;
    float32 slow_coef = .001f;
  };
}  // namespace gestalt