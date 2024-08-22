#include "Camera.hpp"

#include "Camera/OrbitCameraData.hpp"
#include <glm/detail/type_vec2.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/constants.hpp> 
#include <glm/trigonometric.hpp>

#include "UserInput.hpp"

namespace gestalt::application {

  void OrbitCamera::update(float64 delta_seconds, const UserInput& movement, OrbitCameraData& data) {
    auto& camera_data = data;

    glm::mat4 view_matrix = camera_data.get_view_matrix();
    // Orbit around the target
    if (movement.right_mouse_button) {
      const glm::vec2 delta
          = glm::vec2(movement.mouse_position_x, movement.mouse_position_y) - camera_data.mouse_pos;
      camera_data.yaw -= delta.x * camera_data.orbit_speed * static_cast<float>(delta_seconds);
      camera_data.pitch += delta.y * camera_data.orbit_speed * static_cast<float>(delta_seconds);
      camera_data.pitch
          = glm::clamp(camera_data.pitch, -glm::half_pi<float>() + 0.01f,
                       glm::half_pi<float>() - 0.01f);  // Clamp pitch to avoid flipping
    }

    // Zoom in and out
    if (movement.scroll != 0.0f) {
      camera_data.distance
          -= movement.scroll * camera_data.zoom_speed * static_cast<float>(delta_seconds);
      camera_data.distance
          = glm::clamp(camera_data.distance, camera_data.min_distance, camera_data.max_distance);
    }

    // Pan around the target
    if (movement.middle_mouse_button || movement.left_control) {
      const glm::vec2 delta
          = glm::vec2(movement.mouse_position_x, movement.mouse_position_y) - camera_data.mouse_pos;
      const glm::vec3 right = glm::vec3(view_matrix[0][0], view_matrix[1][0], view_matrix[2][0]);
      const glm::vec3 up = glm::vec3(view_matrix[0][1], view_matrix[1][1], view_matrix[2][1]);

      camera_data.target
          -= right * delta.x * camera_data.pan_speed * static_cast<float>(delta_seconds);
      camera_data.target
          += up * delta.y * camera_data.pan_speed * static_cast<float>(delta_seconds);
    }

    camera_data.position
        = camera_data.target
          + glm::vec3(
              camera_data.distance * glm::cos(camera_data.pitch) * glm::sin(camera_data.yaw),
              camera_data.distance * glm::sin(camera_data.pitch),
              camera_data.distance * glm::cos(camera_data.pitch) * glm::cos(camera_data.yaw));
    camera_data.mouse_pos = glm::vec2(movement.mouse_position_x, movement.mouse_position_y);
  }

}  // namespace gestalt::foundation