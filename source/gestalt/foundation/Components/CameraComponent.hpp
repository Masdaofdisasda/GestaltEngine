#pragma once
#include <variant>

#include "Component.hpp"
#include "Camera/AnimationCameraData.hpp"
#include "Camera/CameraPositionerType.hpp"
#include "Camera/CameraProjectionType.hpp"
#include "Camera/FirstPersonCameraData.hpp"
#include "Camera/FreeFlyCameraData.hpp"
#include "Camera/OrbitCameraData.hpp"

namespace gestalt::foundation {

    using CameraData = std::variant<FreeFlyCameraData, FirstPersonCameraData, OrbitCameraData, AnimationCameraData>;

  struct CameraComponent : Component {

    explicit CameraComponent(const CameraProjectionType projection, const CameraPositionerType positioner, const CameraData& camera_data)
        : projection(projection), positioner(positioner), camera_data(camera_data) {
    }

    CameraProjectionType projection;
    const CameraPositionerType positioner;
    glm::mat4 view_matrix{1.0f};
    glm::mat4 projection_matrix{1.0};

    CameraData camera_data;
  };

}  // namespace gestalt