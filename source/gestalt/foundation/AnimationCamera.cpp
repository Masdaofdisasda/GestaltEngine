#include "Camera.hpp"

#include <fmt/core.h>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gestalt::foundation {
  MoveToCamera::MoveToCamera(const glm::vec3& pos, const glm::vec3& angles,
      const glm::vec3& desired_pos, const glm::vec3& desired_angles): positionCurrent_(pos),
    positionDesired_(desired_pos),
    anglesCurrent_(angles),
    anglesDesired_(desired_angles) {}

  void MoveToCamera::update(float64 delta_seconds, const Movement& movement) {
    positionCurrent_ += dampingLinear_ * static_cast<float32>(delta_seconds) * (positionDesired_ - positionCurrent_);

    // normalization is required to avoid "spinning" around the object 2pi times
    anglesCurrent_ = clipAngles(anglesCurrent_);
    anglesDesired_ = clipAngles(anglesDesired_);

    // update angles
    anglesCurrent_ -= angleDelta(anglesCurrent_, anglesDesired_) * dampingEulerAngles_
                      * static_cast<float32>(delta_seconds);

    // normalize new angles
    anglesCurrent_ = clipAngles(anglesCurrent_);

    const glm::vec3 a = glm::radians(anglesCurrent_);

    currentTransform_ = glm::translate(glm::yawPitchRoll(a.y, a.x, a.z), -positionCurrent_);
  }

  void MoveToCamera::setPosition(const glm::vec3& p) { positionCurrent_ = p; }

  void MoveToCamera::setAngles(float pitch, float pan, float roll) {
    anglesCurrent_ = glm::vec3(pitch, pan, roll);
  }

  void MoveToCamera::setAngles(const glm::vec3& angles) { anglesCurrent_ = angles; }

  void MoveToCamera::setDesiredPosition(const glm::vec3& p) { positionDesired_ = p; }

  void MoveToCamera::SetDesiredAngles(float pitch, float pan, float roll) {
    anglesDesired_ = glm::vec3(pitch, pan, roll);
  }

  void MoveToCamera::setDesiredAngles(const glm::vec3& angles) { anglesDesired_ = angles; }

  glm::vec3 MoveToCamera::get_position() const { return positionCurrent_; }

  glm::mat4 MoveToCamera::get_view_matrix() const { return currentTransform_; }

  glm::quat MoveToCamera::get_orientation() const { return glm::quat{radians(anglesCurrent_)};
  }

  float MoveToCamera::ClipAngle(float d) {
    if (d < -180.0f) return d + 360.0f;
    if (d > +180.0f) return d - 360.f;
    return d;
  }

  glm::vec3 MoveToCamera::clipAngles(const glm::vec3& angles) {
    return glm::vec3(std::fmod(angles.x, 360.0f), std::fmod(angles.y, 360.0f),
                     std::fmod(angles.z, 360.0f));
  }

  glm::vec3 MoveToCamera::angleDelta(const glm::vec3& anglesCurrent,
      const glm::vec3& anglesDesired) {
    const glm::vec3 d = clipAngles(anglesCurrent) - clipAngles(anglesDesired);
    return glm::vec3(ClipAngle(d.x), ClipAngle(d.y), ClipAngle(d.z));
  }
}  // namespace gestalt::application