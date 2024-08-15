#include "Camera.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "InputTypes.hpp"

namespace gestalt::foundation {
  OrbitCamera::OrbitCamera(const glm::vec3& target, float distance, float minDistance,
      float maxDistance): target_(target),
                          distance_(distance),
                          minDistance_(minDistance),
                          maxDistance_(maxDistance) {
    update_orientation();
  }

  void OrbitCamera::update(float64 delta_seconds, const Movement& movement) {
    // Orbit around the target
    if (movement.right_mouse_button) {
      const glm::vec2 delta
          = glm::vec2(movement.mouse_position_x, movement.mouse_position_y) - mouse_pos_;
      yaw_ -= delta.x * orbit_speed_ * static_cast<float>(delta_seconds);
      pitch_ += delta.y * orbit_speed_ * static_cast<float>(delta_seconds);
      pitch_ = glm::clamp(pitch_, -glm::half_pi<float>() + 0.01f,
                          glm::half_pi<float>() - 0.01f); // Clamp pitch to avoid flipping
    }

    // Zoom in and out
    if (movement.scroll != 0.0f) {
      distance_ -= movement.scroll * zoom_speed_ * static_cast<float>(delta_seconds);
      distance_ = glm::clamp(distance_, minDistance_, maxDistance_);
    }

    // Pan around the target
    if (movement.middle_mouse_button || movement.left_control) {
      const glm::vec2 delta
          = glm::vec2(movement.mouse_position_x, movement.mouse_position_y) - mouse_pos_;
      const glm::vec3 right
          = glm::vec3(view_matrix_[0][0], view_matrix_[1][0], view_matrix_[2][0]);
      const glm::vec3 up
          = glm::vec3(view_matrix_[0][1], view_matrix_[1][1], view_matrix_[2][1]);

      target_ -= right * delta.x * pan_speed_ * static_cast<float>(delta_seconds);
      target_ += up * delta.y * pan_speed_ * static_cast<float>(delta_seconds);
    }

    update_orientation();
    mouse_pos_ = glm::vec2(movement.mouse_position_x, movement.mouse_position_y);
  }

  glm::mat4 OrbitCamera::get_view_matrix() const { return view_matrix_; }

  glm::vec3 OrbitCamera::get_position() const { return position_; }

  glm::quat OrbitCamera::get_orientation() const { return orientation_; }

  void OrbitCamera::set_target(const glm::vec3& target) {
    target_ = target;
    update_orientation();
  }

  void OrbitCamera::set_distance(float distance) {
    distance_ = glm::clamp(distance, minDistance_, maxDistance_);
    update_orientation();
  }

  void OrbitCamera::set_up_vector(const glm::vec3& up) {
    up_ = up;
    update_orientation();
  }

  void OrbitCamera::update_orientation() {
    // Calculate the new position based on the spherical coordinates
    position_ = target_
                + glm::vec3(distance_ * glm::cos(pitch_) * glm::sin(yaw_),
                            distance_ * glm::sin(pitch_),
                            distance_ * glm::cos(pitch_) * glm::cos(yaw_));

    // Look at the target
    view_matrix_ = glm::lookAtRH(position_, target_, up_);
    orientation_ = glm::quat_cast(view_matrix_);
  }

}  // namespace gestalt::application