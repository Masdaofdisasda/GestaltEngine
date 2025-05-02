#include "VulkanCheck.hpp"

#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <fmt/core.h>

void vk_check(const VkResult result, const char* expr) {
  if (result != VK_SUCCESS) {
    fmt::print("Detected Vulkan error: {} in expression: {}\n", string_VkResult(result), expr);
    throw std::runtime_error(fmt::format("Detected Vulkan error: {} in expression: {}", string_VkResult(result), expr));
  }
}
