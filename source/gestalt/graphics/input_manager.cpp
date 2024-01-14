#include "input_manager.h"

#include <fmt/core.h>

void input_manager::handle_event(const SDL_Event& e, uint32_t window_size_x,
                                 uint32_t window_size_y) {

  if (e.type == SDL_MOUSEMOTION) {
    movement_.mouse_position_x = static_cast<float>(e.motion.x) / window_size_x;
    movement_.mouse_position_y = static_cast<float>(e.motion.y) / window_size_y;
  }

  if (e.type == SDL_MOUSEBUTTONDOWN) {
    if (e.button.button == SDL_BUTTON_LEFT) {
      movement_.left_mouse_button = true;
    }
    if (e.button.button == SDL_BUTTON_RIGHT) {
      movement_.right_mouse_button = true;
    }
  }

  if (e.type == SDL_MOUSEBUTTONUP) {
    if (e.button.button == SDL_BUTTON_LEFT) {
      movement_.left_mouse_button = false;
    }
    if (e.button.button == SDL_BUTTON_RIGHT) {
      movement_.right_mouse_button = false;
    }
  }

  if (e.type == SDL_KEYDOWN) {
    if (e.key.keysym.scancode == SDL_SCANCODE_W) {
      movement_.forward = true;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_S) {
      movement_.backward = true;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_A) {
      movement_.left = true;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_D) {
      movement_.right = true;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_LCTRL) {
      movement_.run = true;
    }
  }

  if (e.type == SDL_KEYUP) {
    if (e.key.keysym.scancode == SDL_SCANCODE_W) {
      movement_.forward = false;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_S) {
      movement_.backward = false;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_A) {
      movement_.left = false;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_D) {
      movement_.right = false;
    }
    if (e.key.keysym.scancode == SDL_SCANCODE_LCTRL) {
      movement_.run = false;
    }
  }
}


void input_manager::init() {
  
}
