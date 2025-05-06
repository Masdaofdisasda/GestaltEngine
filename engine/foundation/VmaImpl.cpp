#include "VulkanCore.hpp"

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable : 4505)  // unreferenced local function
  #pragma warning(disable : 4324)  // alignment
  #pragma warning(disable : 4616)  // unknown warning number
  #pragma warning(disable : 4100)  // unreferenced formal parameter
  #pragma warning(disable : 4189)  // local variable initialized but not referenced
  #pragma warning(disable : 4127)  // conditional expression is constant
#elif defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-function"
  #pragma clang diagnostic ignored "-Wunused-parameter"
  #pragma clang diagnostic ignored "-Wunused-variable"
  #pragma clang diagnostic ignored "-Wmissing-field-initializers"
  #pragma clang diagnostic ignored "-Wsign-compare"
  #pragma clang diagnostic ignored "-Wtype-limits"
  #pragma clang diagnostic ignored "-Wstrict-aliasing"
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-function"
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #pragma GCC diagnostic ignored "-Wunused-variable"
  #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  #pragma GCC diagnostic ignored "-Wsign-compare"
  #pragma GCC diagnostic ignored "-Wtype-limits"
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

// Options that affect generated code
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

#if defined(_MSC_VER)
  #pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
  #pragma GCC diagnostic pop
#endif
