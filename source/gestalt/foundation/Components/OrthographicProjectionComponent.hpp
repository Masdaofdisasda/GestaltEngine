#pragma once
#include <variant>

#include "Component.hpp"
#include "common.hpp"
#include "glm/mat4x4.hpp"
#include "glm/ext/matrix_clip_space.hpp"

namespace gestalt::foundation {

  struct OrthographicProjectionComponent : Component {
  private:
    float32 left_;
    float32 right_;
    float32 bottom_;
    float32 top_;
    float32 near_;
    float32 far_;

  public:
    OrthographicProjectionComponent(float32 left, float32 right, float32 bottom, float32 top,
                                    float32 near, float32 far)
        : left_(left), right_(right), bottom_(bottom), top_(top), near_(near), far_(far) {}

    OrthographicProjectionComponent(float32 xmag, float32 ymag, float32 near, float32 far)
        : left_(-xmag), right_(xmag), bottom_(-ymag), top_(ymag), near_(near), far_(far) {}

    float32 left() const { return left_; }
    void set_left(float32 left) { left_ = left; }

    float32 right() const { return right_; }
    void set_right(float32 right) { right_ = right; }

    float32 bottom() const { return bottom_; }
    void set_bottom(float32 bottom) { bottom_ = bottom; }

    float32 top() const { return top_; }
    void set_top(float32 top) { top_ = top; }

    float32 near() const { return near_; }
    void set_near(float32 near) { near_ = std::max(near, 0.01f); }

    float32 far() const { return far_; }
    void set_far(float32 far) { far_ = std::min(far, 10000.f); }

    [[nodiscard]]

    glm::mat4 projection_matrix() const {
      return glm::orthoRH_ZO(left_, right_, bottom_, top_, near_, far_);
    }
  };

}  // namespace gestalt::foundation