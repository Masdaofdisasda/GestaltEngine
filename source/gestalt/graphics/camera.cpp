#include "camera.h"

#include <fmt/core.h>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


void free_fly_camera::update(double delta_seconds, const movement& movement) {

    auto mouse_pos = glm::vec2(movement.mouse_position_x, movement.mouse_position_y);
  if (movement.right_mouse_button) {
    const glm::vec2 delta = mouse_pos - mouse_pos_;
    glm::quat deltaQuat = glm::quat(glm::vec3(mouse_speed * delta.y, mouse_speed * delta.x, 0.0f));
    glm::quat unclamped_rotation = deltaQuat * camera_orientation_;
    float pitch = glm::pitch(unclamped_rotation);
    float yaw = glm::yaw(unclamped_rotation);

    if ((std::abs(yaw) >= 0.01 || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
      camera_orientation_ = unclamped_rotation;

    camera_orientation_ = glm::normalize(camera_orientation_);
    set_up_vector(up_);
  }
  mouse_pos_ = mouse_pos;

  // translate camera by adding or subtracting to the orthographic vectors
  const glm::mat4 v = mat4_cast(camera_orientation_);

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
  if (movement.run) accel *= fast_coef;

  if (accel == glm::vec3(0)) {
    // decelerate naturally according to the damping value
    move_speed_
        -= move_speed_ * std::min((1.0f / damping) * static_cast<float>(delta_seconds), 1.0f);
  } else {
    // acceleration
    move_speed_ += accel * acceleration * static_cast<float>(delta_seconds);
    const float maxSpeed = movement.run ? max_speed * fast_coef : max_speed;
    if (length(move_speed_) > maxSpeed) move_speed_ = glm::normalize(move_speed_) * maxSpeed;
  }

  camera_position_ += move_speed_ * static_cast<float>(delta_seconds);
}

void free_fly_camera::set_position(const glm::vec3& pos) { camera_position_ = pos; }

void free_fly_camera::set_up_vector(const glm::vec3& up) {
  const glm::mat4 view = get_view_matrix();
  const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
  camera_orientation_ = lookAt(camera_position_, camera_position_ + dir, up);
}