# Assuming VULKAN_SDK is set in your environment
set(VULKAN_SDK $ENV{VULKAN_SDK})
message(STATUS "Vulkan SDK: ${VULKAN_SDK}")

set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK}/Include")
message(STATUS "Vulkan INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")

set(Vulkan_LIBRARIES "${VULKAN_SDK}/Lib")
message(STATUS "Vulkan LIBRARIES: ${Vulkan_LIBRARIES}")

if(WIN32)
    set(VULKAN_LIBRARY "${Vulkan_LIBRARIES}/vulkan-1.lib")
elseif(UNIX AND NOT APPLE)
    set(VULKAN_LIBRARY "${Vulkan_LIBRARIES}/libvulkan.so")
elseif(APPLE)
    set(VULKAN_LIBRARY "${Vulkan_LIBRARIES}/libvulkan.dylib")
endif()

message(STATUS "Vulkan LIBRARY: ${VULKAN_LIBRARY}")

# When linking your target, link against the Vulkan library
#target_link_libraries(your_target PRIVATE ${VULKAN_LIBRARY})

#find_package(Vulkan REQUIRED)
#message(STATUS "Vulkan INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}, LIBRARIES: ${Vulkan_LIBRARIES}")
