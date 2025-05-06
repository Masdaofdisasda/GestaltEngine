#include "InputSystem.hpp"

#include <fmt/core.h>

namespace gestalt::application {

    void InputSystem::handle_event(const SDL_Event& e, uint32 window_size_x,
                                   uint32 window_size_y) {
      switch (e.type) {
        case SDL_MOUSEMOTION:
          handle_mouse_motion(e, window_size_x, window_size_y);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          handle_mouse_button(e);
          break;
        case SDL_MOUSEWHEEL:  // Add this case to handle mouse wheel events
          handle_mouse_wheel(e);
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          handle_keyboard(e);
          break;
        default:
          break;
      }
    }

    void InputSystem::reset_frame() {
      user_input_.scroll = 0;
      user_input_.mouse_position_y_rel = 0.f;
      user_input_.mouse_position_x_rel = 0.f;
    }

    void InputSystem::handle_mouse_motion(const SDL_Event& e, uint32 window_size_x,
                                          uint32 window_size_y) {
      user_input_.mouse_position_x = static_cast<float>(e.motion.x) / window_size_x;
      user_input_.mouse_position_y = static_cast<float>(e.motion.y) / window_size_y;

      user_input_.mouse_position_x_rel = static_cast<float>(e.motion.xrel) / window_size_x;
      user_input_.mouse_position_y_rel = static_cast<float>(e.motion.yrel) / window_size_y;
    }

    void InputSystem::handle_mouse_button(const SDL_Event& e) {
      const bool pressed = (e.type == SDL_MOUSEBUTTONDOWN);
      switch (e.button.button) {
        case SDL_BUTTON_LEFT:
          user_input_.left_mouse_button = pressed;
          break;
        case SDL_BUTTON_RIGHT:
          user_input_.right_mouse_button = pressed;
          break;
        case SDL_BUTTON_MIDDLE:
          user_input_.middle_mouse_button = pressed;
          break;
        default:
          break;
      }
    }

    void InputSystem::handle_mouse_wheel(const SDL_Event& e) {
      user_input_.scroll = static_cast<float32>(e.wheel.y);  // `y` is positive for scroll up and negative for scroll down
    }

    void InputSystem::handle_keyboard(const SDL_Event& e) {
      const bool pressed = (e.type == SDL_KEYDOWN);
      switch (e.key.keysym.scancode) {
        case SDL_SCANCODE_W:
          user_input_.forward = pressed;
          break;
        case SDL_SCANCODE_S:
          user_input_.backward = pressed;
          break;
        case SDL_SCANCODE_A:
          user_input_.left = pressed;
          break;
        case SDL_SCANCODE_D:
          user_input_.right = pressed;
          break;
        case SDL_SCANCODE_SPACE:
          user_input_.up = pressed;
          break;
        case SDL_SCANCODE_LSHIFT:
          user_input_.down = pressed;
          break;
        case SDL_SCANCODE_LCTRL:
          user_input_.left_control = pressed;
          break;
        case SDL_SCANCODE_LALT:
          user_input_.crouch = pressed;
          break;
        default:
          break;
      }
    }
}  // namespace gestalt