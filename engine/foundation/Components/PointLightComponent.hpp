#pragma once

#include "Component.hpp"
#include "common.hpp"
#include "glm/vec3.hpp"

namespace gestalt::foundation {

  struct PointLightComponent : Component {
  private:
    glm::vec3 color_;
    float32 intensity_;
    float32 range_;
    uint32 first_light_view_projection_;  // index into the light view projection buffer the first
                                          // out of 6 faces of the cube map

  public:
    PointLightComponent(glm::vec3 color, float32 intensity, float32 range,
                        uint32 first_light_view_projection)
        : color_(color),
          intensity_(intensity),
          range_(range),
          first_light_view_projection_(first_light_view_projection) {}

    glm::vec3 color() const { return color_; }
    void set_color(const glm::vec3& color) { color_ = color; }

    float32 intensity() const { return intensity_; }
    void set_intensity(const float32 intensity) { intensity_ = intensity; }

    float32 range() const { return range_; }
    void set_range(const float32 range) { range_ = range; }

    uint32 first_light_view_projection() const { return first_light_view_projection_; }
    void set_first_light_view_projection(const uint32 first_light_view_projection) {
      first_light_view_projection_ = first_light_view_projection;
    }
  };

}  // namespace gestalt::foundation