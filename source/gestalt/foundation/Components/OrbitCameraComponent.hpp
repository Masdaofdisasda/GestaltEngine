#pragma once

#include "Component.hpp"
#include "glm/vec3.hpp"
#include <glm/common.hpp>

#include "glm/fwd.hpp"
#include "glm/ext/matrix_transform.hpp"

namespace gestalt::foundation {

  struct OrbitCameraComponent : Component {
  private:

    glm::vec3 target_;
    float32 distance_;
    float32 yaw_;
    float32 pitch_;

    float32 min_distance_ = 1.0f;
    float32 max_distance_ = 100.0f;
    float32 orbit_speed_ = 2.f;
    float32 zoom_speed_ = 0.1f;
    float32 pan_speed_ = 10.0f;

    // State Variables
    glm::vec3 position_ = glm::vec3(0.0f);
    glm::quat orientation_ = glm::quat(glm::vec3(0.0f));
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec2 mouse_pos_ = glm::vec2(0);

  public:
    OrbitCameraComponent( const glm::vec3& target,
                         const float32 distance = 5.f, const float32 yaw = 45.f,
                         const float32 pitch = .3f)
        : yaw_(yaw), pitch_(pitch) {
      set_target(target);
      set_distance(distance);
    }

    glm::vec3 position() const { return position_; }
    glm::quat orientation() const { return orientation_; }

    glm::vec3 target() const { return target_; }
    void set_target(const glm::vec3& target) { this->target_ = target; }

    float32 distance() const { return distance_; }
    void set_distance(const float32 distance) {
      this->distance_ = glm::clamp(distance, min_distance_, max_distance_);
    }

    float32 min_distance() const { return min_distance_; }
    float32 max_distance() const { return max_distance_; }

    float32 yaw() const { return yaw_; }
    void set_yaw(const float32 yaw) { this->yaw_ = yaw; }

    float32 pitch() const { return pitch_; }
    void set_pitch(const float32 pitch) { this->pitch_ = pitch; }

    float32 orbit_speed() const { return orbit_speed_; }
    void set_orbit_speed(const float32 orbit_speed) { this->orbit_speed_ = orbit_speed; }

    float32 zoom_speed() const { return zoom_speed_; }
    void set_zoom_speed(const float32 zoom_speed) { this->zoom_speed_ = zoom_speed; }

    float32 pan_speed() const { return pan_speed_; }
    void set_pan_speed(const float32 pan_speed) { this->pan_speed_ = pan_speed; }

    [[nodiscard]] glm::mat4 view_matrix() const { return lookAtRH(position_, target_, up_); }

    void update(float32 delta_seconds, const UserInput& movement) {
      glm::mat4 view = view_matrix();
      // Orbit around the target
      if (movement.right_mouse_button) {
        const glm::vec2 delta
            = glm::vec2(movement.mouse_position_x, movement.mouse_position_y) - mouse_pos_;
        yaw_ -= delta.x * orbit_speed_ * delta_seconds;
        pitch_ += delta.y * orbit_speed_ * delta_seconds;
        pitch_ = glm::clamp(pitch_, -glm::half_pi<float>() + 0.01f,
                            glm::half_pi<float>() - 0.01f);  // Clamp pitch to avoid flipping
      }

      // Zoom in and out
      if (movement.scroll != 0.0f) {
        distance_ -= movement.scroll * zoom_speed_ * delta_seconds;
        distance_ = glm::clamp(distance_, min_distance_, max_distance_);
      }

      // Pan around the target
      if (movement.middle_mouse_button || movement.left_control) {
        const glm::vec2 delta
            = glm::vec2(movement.mouse_position_x, movement.mouse_position_y) - mouse_pos_;
        const glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
        const glm::vec3 up = glm::vec3(view[0][1], view[1][1], view[2][1]);

        target_ -= right * delta.x * pan_speed_ * delta_seconds;
        target_ += up * delta.y * pan_speed_ * delta_seconds;
      }

      position_
          = target_
            + glm::vec3(distance_ * glm::cos(pitch_) * glm::sin(yaw_), distance_ * glm::sin(pitch_),
                        distance_ * glm::cos(pitch_) * glm::cos(yaw_));
      mouse_pos_ = glm::vec2(movement.mouse_position_x, movement.mouse_position_y);
    }
  };

}  // namespace gestalt::foundation