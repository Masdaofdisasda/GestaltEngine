#pragma once
#include <glm/gtc/quaternion.hpp>

#include "AnimationChannel.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  struct AnimationComponent {
    AnimationChannel<glm::vec3> translation_channel;
    AnimationChannel<glm::quat> rotation_channel;
    AnimationChannel<glm::vec3> scale_channel;
    bool loop = true;

    explicit AnimationComponent(std::vector<Keyframe<glm::vec3>> translation_keyframes,
                                std::vector<Keyframe<glm::quat>> rotation_keyframes,
                                std::vector<Keyframe<glm::vec3>> scale_keyframes)
        : translation_channel(AnimationChannel(std::move(translation_keyframes))),
          rotation_channel(AnimationChannel(std::move(rotation_keyframes))),
          scale_channel(AnimationChannel(std::move(scale_keyframes))) {}
  };

}  // namespace gestalt