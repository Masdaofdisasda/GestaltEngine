#pragma once

namespace gestalt::foundation {

  struct Component {
    mutable bool is_dirty = true;
  };

}  // namespace gestalt::foundation