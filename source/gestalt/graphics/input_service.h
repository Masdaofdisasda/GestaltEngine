#pragma once

#include <SDL2/SDL.h>

struct movement {
  float mouse_position_x{0.f};
  float mouse_position_y{0.f};
  bool left_mouse_button{false};
  bool right_mouse_button{false};
  bool forward{false};
  bool backward{false};
  bool left{false};
  bool right{false};
  bool up{false};
  bool down{false};
  bool run{false};
};

class input_service {

  movement movement_{};

  void handle_mouse_motion(const SDL_Event& e, uint32_t window_size_x, uint32_t window_size_y);
  void handle_mouse_button(const SDL_Event& e);
  void handle_keyboard(const SDL_Event& e);

public:

  void handle_event(const SDL_Event& e, uint32_t window_size_x, uint32_t window_size_y);

  [[nodiscard]] const movement& get_movement() const { return movement_; }
};