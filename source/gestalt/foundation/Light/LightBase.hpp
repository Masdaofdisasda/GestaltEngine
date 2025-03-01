#pragma once
#include "common.hpp"

namespace gestalt::foundation {

  struct LightBase {
  private:
    glm::vec3 color_;
    float32 intensity_;

  public:
    LightBase(glm::vec3 color, float32 intensity) : color_(color), intensity_(intensity) {}

    [[nodiscard]] glm::vec3 color() const { return color_; }
    [[nodiscard]] float32 intensity() const { return intensity_; }
    void set_color(const glm::vec3& color) { color_ = color; }
    void set_intensity(float32 intensity) { intensity_ = intensity; }
  };

}  // namespace gestalt::foundation