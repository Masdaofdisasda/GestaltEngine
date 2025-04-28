#pragma once

#include <VulkanTypes.hpp>

#include <fmt/core.h>

void vk_check(VkResult result, const char* expr);

#ifndef VK_CHECK
  #define VK_CHECK(x) vk_check(x, #x)
#endif
