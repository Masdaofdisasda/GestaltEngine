#pragma once

#include "common.hpp"

#include <SDL.h>


namespace gestalt::application {

    struct Movement {
      float32 mouse_position_x{0.f};
      float32 mouse_position_y{0.f};
      bool left_mouse_button{false};
      bool right_mouse_button{false};
      bool forward{false};
      bool backward{false};
      bool left{false};
      bool right{false};
      bool up{false};
      bool down{false};
      bool run{false};
      bool crouch{false};
    };

    class InputSystem {
      Movement movement_{};

      void handle_mouse_motion(const SDL_Event& e, uint32 window_size_x, uint32 window_size_y);
      void handle_mouse_button(const SDL_Event& e);
      void handle_keyboard(const SDL_Event& e);

    public:
      void handle_event(const SDL_Event& e, uint32 window_size_x, uint32 window_size_y);

      [[nodiscard]] const Movement& get_movement() const { return movement_; }
    };
}  // namespace gestalt