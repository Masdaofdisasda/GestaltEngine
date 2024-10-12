#pragma once
#include <variant>

#include "Component.hpp"
#include "Camera/AnimationCameraData.hpp"
#include "Camera/FirstPersonCameraData.hpp"
#include "Camera/FreeFlyCameraData.hpp"
#include "Camera/OrbitCameraData.hpp"
#include "Camera/OrthographicProjectionData.hpp"
#include "Camera/PerspectiveProjectionData.hpp"

namespace gestalt::foundation {

  using CameraData = std::variant<FreeFlyCameraData, FirstPersonCameraData, OrbitCameraData, AnimationCameraData>;
  using ProjectionData = std::variant<PerspectiveProjectionData, OrthographicProjectionData>;

  struct CameraComponent : Component {

      CameraComponent(const ProjectionData& projection_data, const FreeFlyCameraData& camera_data)
        : projection_data(projection_data),  camera_data(camera_data) {}

      CameraComponent(const ProjectionData& projection_data,
                      const FirstPersonCameraData& camera_data)
          : projection_data(projection_data), camera_data(camera_data) {}

      CameraComponent(const ProjectionData& projection_data, const OrbitCameraData& camera_data)
          : projection_data(projection_data), camera_data(camera_data) {}

      CameraComponent(const ProjectionData& projection_data, const AnimationCameraData& camera_data)
          : projection_data(projection_data), camera_data(camera_data) {}

      glm::mat4 view_matrix{1.0f};
      glm::mat4 projection_matrix{1.0};

      ProjectionData projection_data;
      CameraData camera_data;
    };

}  // namespace gestalt