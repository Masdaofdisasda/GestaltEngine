#pragma once

#include "Component.hpp"
#include "common.hpp"
#include "glm/vec3.hpp"

namespace gestalt::foundation {

  struct SpotLightComponent : Component {
  private:
    glm::vec3 color_;
    float32 intensity_;
    float32 range_;
    uint32 light_view_projection_;  // index into the light view projection buffer
    float32 inner_cone_cos_;
    float32 outer_cone_cos_;

  public:
    SpotLightComponent(glm::vec3 color, float32 intensity, float32 range,
                       uint32 light_view_projection, float32 inner_cone_cos, float32 outer_cone_cos)
        : color_(color),
          intensity_(intensity),
          range_(range),
          light_view_projection_(light_view_projection),
          inner_cone_cos_(inner_cone_cos),
          outer_cone_cos_(outer_cone_cos) {}

    glm::vec3 color() const { return color_; }
    void set_color(const glm::vec3& color) { color_ = color; }

    float32 intensity() const { return intensity_; }
    void set_intensity(const float32 intensity) { intensity_ = intensity; }

    float32 range() const { return range_; }
    void set_range(const float32 range) { range_ = range; }

    uint32 light_view_projection() const { return light_view_projection_; }
    void set_light_view_projection(const uint32 light_view_projection) {
      light_view_projection_ = light_view_projection;
    }

    float32 inner_cone_cos() const { return inner_cone_cos_; }
    void set_inner_cone_cos(float32 inner_cone) { inner_cone_cos_ = inner_cone; }

    float32 outer_cone_cos() const { return outer_cone_cos_; }
    void set_outer_cone_cos(float32 outer_cone) { outer_cone_cos_ = outer_cone; }
  };

}  // namespace gestalt::foundation