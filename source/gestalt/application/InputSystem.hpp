#pragma once

#include "common.hpp"
#include "InputTypes.hpp"

#include <SDL.h>



namespace gestalt::application {

    class InputSystem {
      Movement movement_{};

      void handle_mouse_motion(const SDL_Event& e, uint32 window_size_x, uint32 window_size_y);
      void handle_mouse_button(const SDL_Event& e);
      void handle_mouse_wheel(const SDL_Event& e);
      void handle_keyboard(const SDL_Event& e);

    public:
      void handle_event(const SDL_Event& e, uint32 window_size_x, uint32 window_size_y);
      void reset_frame();

      [[nodiscard]] const Movement& get_movement() const { return movement_; }
    };
}  // namespace gestalt