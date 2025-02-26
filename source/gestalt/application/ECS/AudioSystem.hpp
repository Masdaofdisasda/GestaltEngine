#pragma once

#include "BaseSystem.hpp"
#include <memory>

namespace gestalt::application {

    struct AudioEngine;

    class AudioSystem final : public BaseSystem {
    std::unique_ptr<AudioEngine> audio_engine_; 
    uint32 frame_ = 0;

  public:
    AudioSystem();
    ~AudioSystem() override;

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    void prepare() override;

    void update(float delta_time, const UserInput& movement, float aspect) override;

      void cleanup() override;

    };

}  // namespace gestalt