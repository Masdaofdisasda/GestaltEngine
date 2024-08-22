#pragma once

#include <fmt/core.h>

inline void vk_check(const VkResult result, const char* expr) {
  if (result != VK_SUCCESS) {
    fmt::println("Detected Vulkan error: {} in expression: {}", string_VkResult(result), expr);
    std::abort();
  }
}

#define VK_CHECK(x) vk_check(x, #x)
