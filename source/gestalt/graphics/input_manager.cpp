#include "input_manager.h"

void input_manager::handle_event(const SDL_Event& e, uint32_t window_size_x,
                                 uint32_t window_size_y) {
  if (e.type == SDL_MOUSEMOTION) {
    movement_.mouse_position_x = static_cast<float>(e.motion.x) / window_size_x;
    movement_.mouse_position_y = static_cast<float>(e.motion.y) / window_size_y;
  }

  if (e.type == SDL_KEYDOWN) {
    key_states_[e.key.keysym.sym] = true;
  } else if (e.type == SDL_KEYUP) {
    key_states_[e.key.keysym.sym] = false;
  }
}

void input_manager::set_key_state(const SDL_Keycode key, const bool state) {
  key_states_[key] = state;
  update_movement_state();
}

void input_manager::update_movement_state() {
  if (key_states_[SDLK_w]) movement_.forward = true;
  if (key_states_[SDLK_s]) movement_.backward = true;
  if (key_states_[SDLK_a]) movement_.left = true;
  if (key_states_[SDLK_d]) movement_.right = true;
  if (key_states_[SDLK_SPACE]) movement_.up = true;
  if (key_states_[SDLK_LSHIFT]) movement_.down = true;
}

void input_manager::init() {
  key_states_[SDLK_w] = false;
  key_states_[SDLK_s] = false;
  key_states_[SDLK_a] = false;
  key_states_[SDLK_d] = false;
  key_states_[SDLK_SPACE] = false;
  key_states_[SDLK_LSHIFT] = false;
  key_states_[SDLK_LCTRL] = false;
}

bool input_manager::is_key_pressed(SDL_Keycode key) const {
  auto it = key_states_.find(key);
  return it != key_states_.end() && it->second;
}
