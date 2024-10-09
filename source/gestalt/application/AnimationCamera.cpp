#include "Camera.hpp"

#include "Camera/AnimationCameraData.hpp"
#include <cmath>
#include <glm/detail/type_vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gestalt::application {

  glm::vec3 ClipAngles(const glm::vec3& angles) {
    return {std::fmod(angles.x, 360.0f), std::fmod(angles.y, 360.0f), std::fmod(angles.z, 360.0f)};
  }

  float ClipAngle(float d) {
    if (d < -180.0f) return d + 360.0f;
    if (d > +180.0f) return d - 360.f;
    return d;
  }

  glm::vec3 AngleDelta(const glm::vec3& anglesCurrent, const glm::vec3& anglesDesired) {
    const glm::vec3 d = ClipAngles(anglesCurrent) - ClipAngles(anglesDesired);
    return {ClipAngle(d.x), ClipAngle(d.y), ClipAngle(d.z)};
  }
  void MoveToCamera::update(float32 delta_seconds, const UserInput& movement, 
                            AnimationCameraData& data) {
    auto& camera_data = data;

    glm::quat unclamped_rotation = camera_data.orientation;
    float32 pitch = glm::pitch(unclamped_rotation);
    float32 yaw = glm::yaw(unclamped_rotation);

    if ((std::abs(yaw) >= 0.01 || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
      camera_data.orientation = unclamped_rotation;

    camera_data.orientation = normalize(camera_data.orientation);
  }

}  // namespace gestalt::application