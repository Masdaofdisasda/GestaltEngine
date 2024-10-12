#include "Camera.hpp"

#include "Camera/AnimationCameraData.hpp"
#include <cmath>
#include <glm/detail/type_vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gestalt::application {

  void MoveToCamera::update(float32 delta_seconds, const UserInput& movement, 
                            AnimationCameraData& data) {
    auto& camera_data = data;

    glm::quat unclamped_rotation = camera_data.orientation;
    float32 pitch = glm::pitch(unclamped_rotation);
    float32 yaw = glm::yaw(unclamped_rotation);

    if ((std::abs(yaw) >= 0.0001 || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
      camera_data.orientation = unclamped_rotation;

    camera_data.orientation = normalize(camera_data.orientation);
  }

}  // namespace gestalt::application