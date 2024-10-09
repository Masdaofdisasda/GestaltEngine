#pragma once

namespace gestalt::foundation {
  enum class InterpolationType;

template <typename V> struct Keyframe {
    float32 time;                // Time at which this keyframe occurs
    V value;                   // The value of the property at this keyframe
    InterpolationType type;  // The interpolation method for this keyframe
  };

}  // namespace gestalt