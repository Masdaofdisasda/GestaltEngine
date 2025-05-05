#pragma once

// Only files that really allocate GPU memory should include this.

#include "VulkanCore.hpp"  // for volk / vulkan.h

// Extra VMA compile‑time options
#define VMA_VULKAN_VERSION 1003000

#include <vk_mem_alloc.h> 
