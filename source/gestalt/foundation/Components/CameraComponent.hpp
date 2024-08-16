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

      CameraComponent(const CameraProjectionType projection, const FreeFlyCameraData& camera_data)
          : projection(projection), positioner(kFreeFly), camera_data(camera_data) {}

      CameraComponent(const CameraProjectionType projection,
                      const FirstPersonCameraData& camera_data)
          : projection(projection), positioner(kFirstPerson), camera_data(camera_data) {}

      CameraComponent(const CameraProjectionType projection, const OrbitCameraData& camera_data)
          : projection(projection), positioner(kOrbit), camera_data(camera_data) {}

      CameraComponent(const CameraProjectionType projection, const AnimationCameraData& camera_data)
          : projection(projection), positioner(kAnimation), camera_data(camera_data) {}

      CameraProjectionType projection;
      const CameraPositionerType positioner;
      glm::mat4 view_matrix{1.0f};
      glm::mat4 projection_matrix{1.0};

      CameraData camera_data;
    };

}  // namespace gestalt