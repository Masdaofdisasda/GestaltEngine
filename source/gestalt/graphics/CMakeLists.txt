message(STATUS "Configuring graphics module...")

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
    time_tracker.h
    input_manager.h
    input_manager.cpp
)

target_compile_features(graphics PRIVATE cxx_std_20)

target_include_directories(graphics PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(graphics PRIVATE fmt::fmt vk-bootstrap::vk-bootstrap)
