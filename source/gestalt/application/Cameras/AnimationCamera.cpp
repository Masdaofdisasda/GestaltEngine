#include "AnimationCamera.hpp"

#include "Camera/AnimationCameraData.hpp"
#include <cmath>
#include <glm/detail/type_vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gestalt::application {

  void AnimationCamera::update(float32 delta_seconds, const UserInput& movement, 
                            AnimationCameraData& data) {
    auto& camera_data = data;

    camera_data.orientation = normalize(camera_data.orientation);
  }

}  // namespace gestalt::application