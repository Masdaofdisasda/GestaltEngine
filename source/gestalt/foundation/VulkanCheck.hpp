#pragma once

#include <fmt/core.h>

inline void vk_check(const VkResult result, const char* expr) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(fmt::format("Detected Vulkan error: {} in expression: {}", string_VkResult(result), expr));
  }
}

#define VK_CHECK(x) vk_check(x, #x)
