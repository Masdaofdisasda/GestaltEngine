#include "Camera.hpp"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gestalt::foundation {

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
  void MoveToCamera::update(float64 delta_seconds, const Movement& movement,
                            AnimationCameraData& data) {
    auto& camera_data = data;
    camera_data.position_current += camera_data.damping_linear * static_cast<float32>(delta_seconds)
                                    * (camera_data.position_desired - camera_data.position_current);

    // normalization is required to avoid "spinning" around the object 2pi times
    camera_data.angles_current = ClipAngles(camera_data.angles_current);
    camera_data.angles_desired = ClipAngles(camera_data.angles_desired);

    // update angles
    camera_data.angles_current -= AngleDelta(camera_data.angles_current, camera_data.angles_desired)
                                  * camera_data.damping_euler_angles
                                  * static_cast<float32>(delta_seconds);

    // normalize new angles
    camera_data.angles_current = ClipAngles(camera_data.angles_current);
  }

}  // namespace gestalt::application