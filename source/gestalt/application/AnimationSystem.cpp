#include "SceneSystem.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Animation/AnimationComponent.hpp"

namespace gestalt::application {

  void AnimationSystem::prepare() {
    notification_manager_->subscribe(
        ChangeType::ComponentUpdated, [this](const ChangeEvent& event) {
          if (repository_->transform_components.get(event.entityId).has_value()) {
            updatable_entities_.push_back(event.entityId);
          }
        });
  }

  void AnimationSystem::update_translation(const Entity& entity,
                                           AnimationChannel<glm::vec3>& translation_channel, bool loop) const {
    const auto& translation_keyframes = translation_channel.keyframes;
    float current_time = translation_channel.current_time;
    current_time += delta_time_ * 0.01f;

    if (translation_keyframes.size() < 2) {
      return;  // Not enough keyframes to interpolate
    }

    // Find the keyframe pair that surrounds the current time
    size_t start_index = 0;
    size_t end_index = 1;

    for (size_t i = 0; i < translation_keyframes.size() - 1; ++i) {
      if (current_time >= translation_keyframes[i].time
          && current_time <= translation_keyframes[i + 1].time) {
        start_index = i;
        end_index = i + 1;
        break;
      }
    }

    // Get the two keyframes
    auto& start_keyframe = translation_keyframes[start_index];
    auto& end_keyframe = translation_keyframes[end_index];

    // Compute the interpolation factor (t) between the start and end keyframes
    const float32 t
        = (current_time - start_keyframe.time) / (end_keyframe.time - start_keyframe.time);

    // Linearly interpolate the translation between the two keyframes
    const glm::vec3 interpolated_translation = mix(start_keyframe.value, end_keyframe.value, t);

    repository_->transform_components[entity].position = interpolated_translation;

    if (loop && current_time > translation_keyframes.back().time) {
      current_time = 0.0f;  // Reset for looping
    }
    translation_channel.current_time = current_time;
  }

  void AnimationSystem::update_rotation(const Entity& entity,
                                           AnimationChannel<glm::quat>& rotation_channel, bool loop) const {
    const auto& rotation_keyframes = rotation_channel.keyframes;
    float current_time = rotation_channel.current_time;
    current_time += delta_time_ * 0.01f;

    if (rotation_keyframes.size() < 2) {
      return;  // Not enough keyframes to interpolate
    }

    // Find the keyframe pair that surrounds the current time
    size_t start_index = 0;
    size_t end_index = 1;

    for (size_t i = 0; i < rotation_keyframes.size() - 1; ++i) {
      if (current_time >= rotation_keyframes[i].time
          && current_time <= rotation_keyframes[i + 1].time) {
        start_index = i;
        end_index = i + 1;
        break;
      }
    }

    // Get the two keyframes
    auto& start_keyframe = rotation_keyframes[start_index];
    auto& end_keyframe = rotation_keyframes[end_index];

    // Compute the interpolation factor (t) between the start and end keyframes
    const float32 t
        = (current_time - start_keyframe.time) / (end_keyframe.time - start_keyframe.time);

    // Linearly interpolate the translation between the two keyframes
    const glm::quat interpolated_rotation = mix(start_keyframe.value, end_keyframe.value, t);

    repository_->transform_components[entity].rotation = interpolated_rotation;

    if (loop && current_time > rotation_keyframes.back().time) {
      current_time = 0.0f;  // Reset for looping
    }
    rotation_channel.current_time = current_time;
  }

  void AnimationSystem::update() {
    for (auto [entity, animation_component] : repository_->animation_components.asVector()) {

      update_translation(entity, animation_component.get().translation_channel, animation_component.get().loop);
      update_rotation(entity, animation_component.get().rotation_channel,
                         animation_component.get().loop);

    }
  }


  void AnimationSystem::cleanup() {}

}  // namespace gestalt::application