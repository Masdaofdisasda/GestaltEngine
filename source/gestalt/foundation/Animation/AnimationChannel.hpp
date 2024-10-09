#pragma once
#include <vector>

#include "Keyframe.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  template <typename K> struct AnimationChannel {
    std::vector<Keyframe<K>> keyframes;
    uint32 keyframe_index = 0;

    explicit AnimationChannel(std::vector<Keyframe<K>>& keyframes) : keyframes(keyframes) {}

  };

}  // namespace gestalt