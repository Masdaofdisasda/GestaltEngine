#pragma once
#include <limits>

#include "common.hpp"

namespace gestalt::foundation {

    constexpr auto kUnusedTexture = std::numeric_limits<uint16>::max();
    constexpr uint32 kAlbedoTextureFlag = 0x01;
    constexpr uint32 kMetalRoughTextureFlag = 0x02;
    constexpr uint32 kNormalTextureFlag = 0x04;
    constexpr uint32 kEmissiveTextureFlag = 0x08;
    constexpr uint32 kOcclusionTextureFlag = 0x10;
}  // namespace gestalt