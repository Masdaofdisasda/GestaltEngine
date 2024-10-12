#pragma once
#include <vector>

#include "Keyframe.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  template <typename K> struct AnimationChannel {
    std::vector<Keyframe<K>> keyframes;
    float32 current_time = 0.0f;  // Current time in the animation

    explicit AnimationChannel(std::vector<Keyframe<K>> keyframes) : keyframes(keyframes) {}

  };

}  // namespace gestalt