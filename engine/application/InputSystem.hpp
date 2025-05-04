#pragma once

#include "common.hpp"

#include <SDL.h>

#include "UserInput.hpp"


namespace gestalt::application {

    class InputSystem {
      UserInput user_input_{};

      void handle_mouse_motion(const SDL_Event& e, uint32 window_size_x, uint32 window_size_y);
      void handle_mouse_button(const SDL_Event& e);
      void handle_mouse_wheel(const SDL_Event& e);
      void handle_keyboard(const SDL_Event& e);

    public:
      void handle_event(const SDL_Event& e, uint32 window_size_x, uint32 window_size_y);
      void reset_frame();

      [[nodiscard]] const UserInput& get_movement() const { return user_input_; }
    };
}  // namespace gestalt