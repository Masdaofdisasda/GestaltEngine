﻿#pragma once
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>

class time_tracker {
public:
  time_tracker() {
    reset_timer();
  }

  void update_timer() {
    Uint64 newTimeStamp = SDL_GetPerformanceCounter();
    delta_seconds_ = static_cast<float>(static_cast<double>(newTimeStamp - time_stamp_)
                                      / static_cast<double>(SDL_GetPerformanceFrequency()));
    time_stamp_ = newTimeStamp;
  }

  float get_delta_time() const { return delta_seconds_; }

  void reset_timer() {
    time_stamp_ = SDL_GetPerformanceCounter();
    delta_seconds_ = 0.0f;
  }

private:
  Uint64 time_stamp_;    // Stores the last timestamp
  float delta_seconds_;  // Stores the time elapsed since the last frame
};