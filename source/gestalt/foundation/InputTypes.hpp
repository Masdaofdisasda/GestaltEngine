#pragma once

namespace gestalt::foundation {
	  
    struct Movement {
      float32 mouse_position_x{0.f};
      float32 mouse_position_y{0.f};
      float32 mouse_position_x_rel{0.f};
      float32 mouse_position_y_rel{0.f};
      float32 scroll{0.f};
      bool left_mouse_button{false};
      bool right_mouse_button{false};
      bool middle_mouse_button{false};
      bool forward{false};
      bool backward{false};
      bool left{false};
      bool right{false};
      bool up{false};
      bool down{false};
      bool left_control{false};
      bool crouch{false};
    };

}  // namespace gestalt