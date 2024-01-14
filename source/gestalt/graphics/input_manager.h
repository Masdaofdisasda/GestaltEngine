#pragma once

#include <SDL2/SDL.h>

#include <unordered_map>

struct movement {
  float mouse_position_x{0.f};
  float mouse_position_y{0.f};
  bool forward{false};
  bool backward{false};
  bool left{false};
  bool right{false};
  bool up{false};
  bool down{false};
};

class input_manager {
  std::unordered_map<SDL_Keycode, bool> key_states_;
  movement movement_{};

  void set_key_state(SDL_Keycode key, bool state);
  void update_movement_state();

public:
  void init();

  void handle_event(const SDL_Event& e, uint32_t window_size_x, uint32_t window_size_y);

  [[nodiscard]] bool is_key_pressed(SDL_Keycode key) const;

  movement& get_movement() { return movement_; }
};