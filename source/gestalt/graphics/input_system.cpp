#include "input_system.h"

#include <fmt/core.h>


void input_system::handle_event(const SDL_Event& e, uint32_t window_size_x,
                                 uint32_t window_size_y) {
  switch (e.type) {
    case SDL_MOUSEMOTION:
      handle_mouse_motion(e, window_size_x, window_size_y);
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      handle_mouse_button(e);
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      handle_keyboard(e);
      break;
    default:
      break;
  }
}

void input_system::handle_mouse_motion(const SDL_Event& e, uint32_t window_size_x,
                                        uint32_t window_size_y) {
  movement_.mouse_position_x = static_cast<float>(e.motion.x) / window_size_x;
  movement_.mouse_position_y = static_cast<float>(e.motion.y) / window_size_y;
}

void input_system::handle_mouse_button(const SDL_Event& e) {
  bool pressed = (e.type == SDL_MOUSEBUTTONDOWN);
  switch (e.button.button) {
    case SDL_BUTTON_LEFT:
      movement_.left_mouse_button = pressed;
      break;
    case SDL_BUTTON_RIGHT:
      movement_.right_mouse_button = pressed;
      break;
    default:
      break;
  }
}

void input_system::handle_keyboard(const SDL_Event& e) {
  bool pressed = (e.type == SDL_KEYDOWN);
  switch (e.key.keysym.scancode) {
    case SDL_SCANCODE_W:
      movement_.forward = pressed;
      break;
    case SDL_SCANCODE_S:
      movement_.backward = pressed;
      break;
    case SDL_SCANCODE_A:
      movement_.left = pressed;
      break;
    case SDL_SCANCODE_D:
      movement_.right = pressed;
      break;
    case SDL_SCANCODE_SPACE:
      movement_.up = pressed;
      break;
    case SDL_SCANCODE_LSHIFT:
      movement_.down = pressed;
      break;
    case SDL_SCANCODE_LCTRL:
      movement_.run = pressed;
      break;
    case SDL_SCANCODE_LALT:
      movement_.crouch = pressed;
      break;
    default:
      break;
  }
}
