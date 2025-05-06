#pragma once

#include "Component.hpp"
#include "UserInput.hpp"
#include "glm/vec3.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

namespace gestalt::foundation {

  struct FreeFlyCameraComponent : Component {
  private:
    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::quat orientation_ = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

    float32 mouse_speed_ = 4.5f;
    float32 acceleration_ = 35.f;
    float32 damping_ = 0.15f;
    float32 max_speed_ = 10.f;
    float32 fast_coef_ = 5.f;
    float32 slow_coef_ = .1f;

    // State Variables
    glm::vec3 move_speed_ = glm::vec3(0.0f);
    glm::vec2 mouse_pos_ = glm::vec2(0);

  public:
    FreeFlyCameraComponent(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) {
      set_position(pos);
      set_orientation(target, up);
      set_up_vector(up);
    }

    void set_up_vector(const glm::vec3& up) {
      up_ = up;
      const glm::mat4 view = view_matrix();
      const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
      orientation_ = lookAtRH(position_, position_ + dir, up);
    }

    glm::vec3 position() const { return position_; }
    void set_position(const glm::vec3& pos) { position_ = pos; }

    glm::quat orientation() const { return orientation_; }
    void set_orientation(const glm::vec3& target, const glm::vec3& up) {
      orientation_ = quat_cast(lookAtRH(position_, target, up));
    }

    [[nodiscard]] glm::mat4 view_matrix() const {
      const glm::mat4 t = translate(glm::mat4(1.0f), -position_);
      const glm::mat4 r = mat4_cast(orientation_);
      return r * t;
    }

    void reset_mouse_position(const glm::vec2& p) { mouse_pos_ = p; }

    float32 mouse_speed() const { return mouse_speed_; }
    void set_mouse_speed(float32 speed) { mouse_speed_ = speed; }

    float32 acceleration() const { return acceleration_; }
    void set_acceleration(float32 accel) { acceleration_ = accel; }

    float32 damping() const { return damping_; }
    void set_damping(float32 damp) { damping_ = damp; }

    float32 max_speed() const { return max_speed_; }
    void set_max_speed(float32 speed) { max_speed_ = speed; }

    float32 fast_coef() const { return fast_coef_; }
    void set_fast_coef(float32 coef) { fast_coef_ = coef; }

    float32 slow_coef() const { return slow_coef_; }
    void set_slow_coef(float32 coef) { slow_coef_ = coef; }

    void update(float32 delta_seconds, const UserInput& movement) {
      auto mouse_pos = glm::vec2(movement.mouse_position_x, movement.mouse_position_y);
      if (movement.right_mouse_button) {
        const glm::vec2 delta = mouse_pos - mouse_pos_;
        glm::quat deltaQuat
            = glm::quat(glm::vec3(mouse_speed_ * delta.y, mouse_speed_ * delta.x, 0.0f));
        glm::quat unclamped_rotation = deltaQuat * orientation_;
        float32 pitch = glm::pitch(unclamped_rotation);
        float32 yaw = glm::yaw(unclamped_rotation);

        if ((std::abs(yaw) >= 0.01
             || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
          orientation_ = unclamped_rotation;

        orientation_ = glm::normalize(orientation_);
        set_up_vector(up_);
      }
      mouse_pos_ = mouse_pos;

      // translate camera by adding or subtracting to the orthographic vectors
      const glm::mat4 v = glm::mat4_cast(orientation_);

      const glm::vec3 forward = -glm::vec3(v[0][2], v[1][2], v[2][2]);
      const glm::vec3 right = glm::vec3(v[0][0], v[1][0], v[2][0]);
      const glm::vec3 up = cross(right, forward);

      glm::vec3 accel(0.0f);

      if (movement.forward) accel += forward;
      if (movement.backward) accel -= forward;
      if (movement.left) accel -= right;
      if (movement.right) accel += right;
      if (movement.up) accel += up;
      if (movement.down) accel -= up;
      if (movement.left_control) accel *= fast_coef_;
      if (movement.crouch) accel *= slow_coef_;

      if (accel == glm::vec3(0)) {
        // decelerate naturally according to the damping value
        move_speed_ -= move_speed_ * std::min((1.0f / damping_) * delta_seconds, 1.0f);
      } else {
        // acceleration
        move_speed_ += accel * acceleration_ * delta_seconds;
        const float maxSpeed = movement.left_control ? max_speed_ * fast_coef_ : max_speed_;
        if (glm::length(move_speed_) > maxSpeed)
          move_speed_ = glm::normalize(move_speed_) * maxSpeed;
      }

      position_ += move_speed_ * delta_seconds;
    }
  };

}  // namespace gestalt::foundation