#pragma once
#include <limits>

#include "common.hpp"

namespace gestalt::foundation {

    using Entity = uint32;
    constexpr Entity root_entity = 0;
    constexpr uint32 invalid_entity = std::numeric_limits<uint32>::max();
    constexpr size_t no_component = std::numeric_limits<size_t>::max();
    constexpr size_t default_material = 0;


}  // namespace gestalt