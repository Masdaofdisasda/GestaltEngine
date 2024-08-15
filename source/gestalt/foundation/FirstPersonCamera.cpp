#include "Camera.hpp"

#include <fmt/core.h>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "InputTypes.hpp"

namespace gestalt::foundation {

  FirstPersonCamera::FirstPersonCamera(const glm::vec3& startPosition, const glm::vec3& startUp) {
    set_position(startPosition);
    set_orientation(startPosition + glm::vec3(0.0f, 0.0f, -1.0f), startUp);
    set_up_vector(startUp);
  }

  void FirstPersonCamera::update(float64 delta_seconds, const Movement& movement) {
    const auto mouse_pos = glm::vec2(movement.mouse_position_x_rel, movement.mouse_position_y_rel);

    const glm::vec2 delta = mouse_pos - mouse_pos_;
    glm::quat deltaQuat
        = glm::quat(glm::vec3(mouse_speed_ * delta.y, mouse_speed_ * delta.x, 0.0f));
    glm::quat unclamped_rotation = deltaQuat * camera_orientation_;
    const float32 pitch = glm::pitch(unclamped_rotation);
    const float32 yaw = glm::yaw(unclamped_rotation);

    if ((std::abs(yaw) >= 0.01 || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
      camera_orientation_ = unclamped_rotation;

    camera_orientation_ = normalize(camera_orientation_);
    set_up_vector(up_);
  }

  glm::mat4 FirstPersonCamera::get_view_matrix() const {
    const glm::mat4 t = translate(glm::mat4(1.0f), -camera_position_);
    const glm::mat4 r = mat4_cast(camera_orientation_);
    return r * t;
  }

  glm::vec3 FirstPersonCamera::get_position() const { return camera_position_; }

  glm::quat FirstPersonCamera::get_orientation() const { return camera_orientation_; }

  void FirstPersonCamera::set_position(const glm::vec3& pos) {
    camera_position_ = pos;
  }

  void FirstPersonCamera::set_orientation(const glm::vec3& target, const glm::vec3& up) {
    const glm::mat4 viewMatrix = lookAtRH(camera_position_, target, up);
    camera_orientation_ = quat_cast(viewMatrix);
  }

  void FirstPersonCamera::set_up_vector(const glm::vec3& up) {
    up_ = up;
    const glm::mat4 view = get_view_matrix();
    const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
    camera_orientation_ = lookAtRH(camera_position_, camera_position_ + dir, up);
  }

}  // namespace gestalt::application