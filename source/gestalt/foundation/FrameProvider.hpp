#pragma once

#include "common.hpp"
#include "EngineConfiguration.hpp"

namespace gestalt::foundation {

  struct FrameProvider {

    explicit FrameProvider(const uint64* frame_number) : frame_number(frame_number) {}

    // for frames in flight
    [[nodiscard]] uint8 get_current_frame_index() const {
      return static_cast<uint8>(*frame_number % getFramesInFlight());
    }

    // for overall frames rendered
    [[nodiscard]] uint32 get_current_frame_number() const {
      return static_cast<uint32>(*frame_number);
    }

  private:
    const uint64* frame_number;
  };
}  // namespace gestalt