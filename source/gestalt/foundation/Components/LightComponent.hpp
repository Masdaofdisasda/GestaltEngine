#pragma once
#include <variant>

#include "Component.hpp"
#include "Light/DirectionalLightData.hpp"
#include "Light/LightBase.hpp"
#include "Light/LightType.hpp"
#include "Light/PointLightData.hpp"
#include "Light/SpotLightData.hpp"

namespace gestalt::foundation {

  using LightSpecificData = std::variant<DirectionalLightData, PointLightData, SpotLightData>;

  struct LightComponent : Component {
    LightType type;
    LightBase base;
    LightSpecificData specific;

    LightComponent(glm::vec3 color, float32 intensity, uint32 light_view_projection)
        : type(LightType::kDirectional),
          base{color, intensity},
          specific(DirectionalLightData{light_view_projection}) {}

    LightComponent(glm::vec3 color, float32 intensity, float32 range,
                   uint32 first_light_view_projection)
        : type(LightType::kPoint),
          base{color, intensity},
          specific(PointLightData{range, first_light_view_projection}) {}

    LightComponent(glm::vec3 color, float32 intensity, float32 inner_cone, float32 outer_cone)
        : type(LightType::kSpot),
          base{color, intensity},
          specific(SpotLightData{inner_cone, outer_cone}) {}
  };

}  // namespace gestalt::foundation