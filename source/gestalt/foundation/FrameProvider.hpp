#pragma once

#include "common.hpp"
#include "EngineConfig.hpp"

namespace gestalt::foundation {

  struct FrameProvider {

    explicit FrameProvider(const uint64* frame_number) : frame_number(frame_number) {}

    uint8 get_current_frame_index() const {
      return static_cast<uint8>(*frame_number % getFramesInFlight());
    }

  private:
    const uint64* frame_number;
  };
}  // namespace gestalt