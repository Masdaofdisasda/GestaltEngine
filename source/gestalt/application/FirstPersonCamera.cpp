#include "Camera.hpp"

#include "Camera/FirstPersonCameraData.hpp"
#include <glm/gtx/quaternion.hpp>

#include "UserInput.hpp"

namespace gestalt::application {

  void FirstPersonCamera::update(float64 delta_seconds, const UserInput& movement,
                                 FirstPersonCameraData& data) {
    auto& camera_data = data;
    const auto mouse_pos = glm::vec2(movement.mouse_position_x_rel, movement.mouse_position_y_rel);

    const glm::vec2 delta = mouse_pos - camera_data.mouse_pos;
    glm::quat deltaQuat = glm::quat(
        glm::vec3(camera_data.mouse_speed * delta.y, camera_data.mouse_speed * delta.x, 0.0f));
    glm::quat unclamped_rotation = deltaQuat * camera_data.orientation;
    const float32 pitch = glm::pitch(unclamped_rotation);
    const float32 yaw = glm::yaw(unclamped_rotation);

    if ((std::abs(yaw) >= 0.01 || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
      camera_data.orientation = unclamped_rotation;

    camera_data.orientation = normalize(camera_data.orientation);
    camera_data.set_up_vector(camera_data.up);
  }

}  // namespace gestalt::application