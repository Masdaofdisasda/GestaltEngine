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
  private:
    LightType type_;
    LightBase base_;
    LightSpecificData specific_;

  public:
    LightComponent(glm::vec3 color, float32 intensity, uint32 light_view_projection)
        : type_(LightType::kDirectional),
          base_{color, intensity},
          specific_(DirectionalLightData{light_view_projection}) {}

    LightComponent(glm::vec3 color, float32 intensity, float32 range,
                   uint32 first_light_view_projection)
        : type_(LightType::kPoint),
          base_{color, intensity},
          specific_(PointLightData{range, first_light_view_projection}) {}

    LightComponent(glm::vec3 color, float32 intensity, float32 range,
                   uint32 light_view_projection, float32 inner_cone_cos, float32 outer_cone_cos)
        : type_(LightType::kSpot),
          base_{color, intensity},
          specific_(SpotLightData{range, light_view_projection, inner_cone_cos, outer_cone_cos}) {}

    glm::vec3 color() const { return base_.color(); }
    float32 intensity() const { return base_.intensity(); }
    void set_color(const glm::vec3& color) { base_.set_color(color); }
    void set_intensity(const float32 intensity) { base_.set_intensity(intensity); }
    LightSpecificData specific() const { return specific_; }

    bool is_type(const LightType type) const { return type_ == type; }
  };

}  // namespace gestalt::foundation