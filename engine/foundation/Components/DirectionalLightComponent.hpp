#pragma once

#include "Component.hpp"
#include "common.hpp"
#include "glm/vec3.hpp"

namespace gestalt::foundation {

  struct DirectionalLightComponent : Component {
  private:
    glm::vec3 color_;
    float32 intensity_;
    uint32 light_view_projection_;

  public:
    DirectionalLightComponent(glm::vec3 color, float32 intensity, uint32 light_view_projection)
        : color_(color), intensity_(intensity), light_view_projection_(light_view_projection) {}

    glm::vec3 color() const { return color_; }
    void set_color(const glm::vec3& color) { color_ = color; }

    float32 intensity() const { return intensity_; }
    void set_intensity(const float32 intensity) { intensity_ = intensity; }

    uint32 light_view_projection() const { return light_view_projection_; }
    void set_light_view_projection(const uint32 light_view_projection) {
      light_view_projection_ = light_view_projection;
    }
  };

}  // namespace gestalt::foundation