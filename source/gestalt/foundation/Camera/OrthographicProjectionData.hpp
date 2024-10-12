#pragma once

#include "common.hpp"

namespace gestalt::foundation {
  struct OrthographicProjectionData {
    OrthographicProjectionData(float32 left, float32 right, float32 bottom, float32 top,
                               float32 near, float32 far)
        : left(left), right(right), bottom(bottom), top(top), near(near), far(far) {}

    OrthographicProjectionData(float32 xmag, float32 ymag, float32 near, float32 far)
        : left(-xmag), right(xmag), bottom(-ymag), top(ymag), near(near), far(far) {}

    float32 left;
    float32 right;
    float32 bottom;
    float32 top;
    float32 near;
    float32 far;
  };
}  // namespace gestalt