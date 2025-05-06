#pragma once

// Enable volk and disable native prototypes
#define VK_NO_PROTOTYPES
#include <volk.h>

// Khronos helper for enum → string
#include <vulkan/vk_enum_string_helper.h>
