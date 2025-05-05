#include "VulkanCore.hpp"

// Options that affect generated code
#pragma warning(push)
#pragma warning(disable : 4505)  // VMA is not my responsibility
#pragma warning(disable : 4324) 
#pragma warning(disable : 4616) 
#pragma warning(disable : 4100)  
#pragma warning(disable : 4189) 
#pragma warning(disable : 4127)

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#if 0
#  define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#  define VMA_DEBUG_LOG_FORMAT(format, ...) \
    do {                                    \
      printf((format), __VA_ARGS__);        \
      printf("\n");                         \
    } while (false)
#endif

#include <vk_mem_alloc.h>
#pragma warning( pop )
