#include "Camera.hpp"

#include "Camera/FreeFlyCameraData.hpp"
#include <glm/gtx/quaternion.hpp>

#include "UserInput.hpp"
#include <utility>

namespace gestalt::application {
  void FreeFlyCamera::update(float64 delta_seconds, const UserInput& movement,
                             FreeFlyCameraData& data) {
    auto& camera_data = data;

    auto mouse_pos = glm::vec2(movement.mouse_position_x, movement.mouse_position_y);
    if (movement.right_mouse_button) {
      const glm::vec2 delta = mouse_pos - camera_data.mouse_pos;
      glm::quat deltaQuat = glm::quat(
          glm::vec3(camera_data.mouse_speed * delta.y, camera_data.mouse_speed * delta.x, 0.0f));
      glm::quat unclamped_rotation = deltaQuat * camera_data.orientation;
      float32 pitch = glm::pitch(unclamped_rotation);
      float32 yaw = glm::yaw(unclamped_rotation);

      if ((std::abs(yaw) >= 0.01
           || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
        camera_data.orientation = unclamped_rotation;

      camera_data.orientation = normalize(camera_data.orientation);
      camera_data.set_up_vector(camera_data.up);
    }
    camera_data.mouse_pos = mouse_pos;

    // translate camera by adding or subtracting to the orthographic vectors
    const glm::mat4 v = mat4_cast(camera_data.orientation);

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
    if (movement.left_control) accel *= camera_data.fast_coef;
    if (movement.crouch) accel *= camera_data.slow_coef;

    if (accel == glm::vec3(0)) {
      // decelerate naturally according to the damping value
      camera_data.move_speed
          -= camera_data.move_speed
             * std::min((1.0f / camera_data.damping) * static_cast<float>(delta_seconds), 1.0f);
    } else {
      // acceleration
      camera_data.move_speed += accel * camera_data.acceleration * static_cast<float>(delta_seconds);
      if (movement.left_control) {
        float maxSpeed = camera_data.max_speed * camera_data.fast_coef;
      } else if (movement.crouch) {
        float maxSpeed = camera_data.max_speed * camera_data.slow_coef;
      } else {
        float maxSpeed = camera_data.max_speed;
      }
      const float maxSpeed = movement.left_control ? camera_data.max_speed * camera_data.fast_coef
                                                   : camera_data.max_speed;
      if (length(camera_data.move_speed) > maxSpeed)
        camera_data.move_speed = normalize(camera_data.move_speed) * maxSpeed;
    }

    camera_data.position += camera_data.move_speed * static_cast<float32>(delta_seconds);
  }

}  // namespace gestalt::application