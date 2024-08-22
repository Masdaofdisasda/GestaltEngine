#pragma once
#include "common.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gestalt::foundation {

  struct OrbitCameraData {
    explicit OrbitCameraData(const glm::vec3& target, const float32 distance = 5.f, const float32 yaw = 45.f, const float32 pitch = .3f)
        : yaw(yaw),
          pitch(pitch) {
      set_target(target);
      set_distance(distance);
    }

    void set_target(const glm::vec3& target) {
      this->target = target;
    }

    void set_distance(const float32 distance) {
      this->distance = glm::clamp(distance, min_distance, max_distance);
    }


    [[nodiscard]] glm::mat4 get_view_matrix() const {
      return lookAtRH(position, target, up);
    }

    // Configuration Values
    glm::vec3 target;
    float32 distance;
    float32 yaw;
    float32 pitch;

    // Adjustable Parameters
    float32 min_distance = 1.0f;
    float32 max_distance = 100.0f;
    float32 orbit_speed = 2.f;
    float32 zoom_speed = 0.1f;
    float32 pan_speed = 10.0f;

    // State Variables 
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat orientation = glm::quat(glm::vec3(0.0f));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec2 mouse_pos = glm::vec2(0);
  };
}  // namespace gestalt