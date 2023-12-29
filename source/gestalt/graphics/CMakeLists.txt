# Create a library for the module
add_library(graphics
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_images.h
    vk_images.cpp 
    vk_descriptors.h
    vk_descriptors.cpp
    vk_pipelines.h
    vk_pipelines.cpp
    vk_engine.h
    vk_engine.cpp
    vk_loader.h
    vk_loader.cpp
    camera.cpp
    camera.h
)

# Specify C++20 for modules
target_compile_features(graphics PRIVATE cxx_std_20)

target_include_directories(graphics PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
# Add Vulkan include directories to the graphics target
target_include_directories(graphics PUBLIC ${Vulkan_INCLUDE_DIRS})

target_sources(graphics PRIVATE
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_images.h
    vk_images.cpp 
    vk_descriptors.h
    vk_descriptors.cpp
    vk_pipelines.h
    vk_pipelines.cpp
    vk_engine.h
    vk_engine.cpp
    vk_loader.h
    vk_loader.cpp
    camera.cpp
    camera.h
)