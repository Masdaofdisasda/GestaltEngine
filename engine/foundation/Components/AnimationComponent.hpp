#pragma once
#include <glm/gtc/quaternion.hpp>

#include "Animation/AnimationChannel.hpp"

namespace gestalt::foundation {

  class AnimationComponent {
    AnimationChannel<glm::vec3> translation_channel_;
    AnimationChannel<glm::quat> rotation_channel_;
    AnimationChannel<glm::vec3> scale_channel_;
    bool loop_ = true;

  public:
    explicit AnimationComponent(std::vector<Keyframe<glm::vec3>> translation_keyframes,
                                std::vector<Keyframe<glm::quat>> rotation_keyframes,
                                std::vector<Keyframe<glm::vec3>> scale_keyframes)
        : translation_channel_(AnimationChannel(std::move(translation_keyframes))),
          rotation_channel_(AnimationChannel(std::move(rotation_keyframes))),
          scale_channel_(AnimationChannel(std::move(scale_keyframes))) {}

    AnimationChannel<glm::vec3>& translation_channel() { return translation_channel_; }
    AnimationChannel<glm::quat>& rotation_channel() { return rotation_channel_; }
    AnimationChannel<glm::vec3>& scale_channel() { return scale_channel_; }
    bool loop() const { return loop_; }
  };

}  // namespace gestalt