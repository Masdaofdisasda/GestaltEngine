#pragma once

#include "common.hpp"
#include "Components.hpp"

#include <glm/gtc/quaternion.hpp>


namespace gestalt::foundation {
  struct Movement;

  class FreeFlyCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, FreeFlyCameraData& data);

    [[nodiscard]] static glm::mat4 get_view_matrix(const FreeFlyCameraData& data);
  };

  class OrbitCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, OrbitCameraData& data);

    [[nodiscard]] static glm::mat4 get_view_matrix(const OrbitCameraData& data);
  };

  class FirstPersonCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, FirstPersonCameraData& data);

    [[nodiscard]] static glm::mat4 get_view_matrix(const FirstPersonCameraData& data);
  };

  class MoveToCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, AnimationCameraData& data);

    static glm::mat4 get_view_matrix(const AnimationCameraData& data);
  };

}  // namespace gestalt::foundation