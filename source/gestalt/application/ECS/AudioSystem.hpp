#pragma once

#include "BaseSystem.hpp"
#include <memory>

namespace gestalt::application {

    struct AudioEngine;

    class AudioSystem final : public BaseSystem, public NonCopyable<AudioSystem> {
    std::unique_ptr<AudioEngine> audio_engine_; 
    uint32 frame_ = 0;

  public:
    AudioSystem();
    void prepare() override;

    void update(float delta_time, const UserInput& movement, float aspect) override;

      void cleanup() override;

    ~AudioSystem() override;
    };

}  // namespace gestalt