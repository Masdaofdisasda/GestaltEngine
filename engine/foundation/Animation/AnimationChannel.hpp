#pragma once
#include <vector>

#include "Keyframe.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  template <typename K> class AnimationChannel {
  public:
    explicit AnimationChannel(std::vector<Keyframe<K>> keyframes) : keyframes_(keyframes) {}

    std::vector<Keyframe<K>> keyframes() { return keyframes_; }
    float32 current_time() const { return current_time_; }
    void set_current_time(const float32 current_time) { current_time_ = current_time; }

  private:
    std::vector<Keyframe<K>> keyframes_;
    float32 current_time_{0.0f};
  };

}  // namespace gestalt