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
    scene_manager.h
    scene_manager.cpp
    camera.cpp
    camera.h
    time_tracking_service.h
    input_system.h
    input_system.cpp
    resource_manager.h
    resource_manager.cpp
    sdl_window.h
    sdl_window.cpp
    vk_deletion_service.h
    vk_gpu.h
    vk_gpu.cpp
	vk_swapchain.h
vk_swapchain.cpp
vk_commands.h
vk_commands.cpp
vk_sync.h
vk_sync.cpp
materials.h
imgui_gui.h
imgui_gui.cpp
gui_actions.h
vk_renderer.h
vk_renderer.cpp
scene_components.h
cubemap_util.cpp
cubemap_util.h
asset_loader.h
asset_loader.cpp
database.h
database.cpp
)

target_compile_features(graphics PRIVATE cxx_std_20)

target_include_directories(graphics PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${Vulkan_INCLUDE_DIRS}
	${Stb_SOURCE_DIR}
)

target_link_libraries(graphics PRIVATE fmt::fmt vk-bootstrap::vk-bootstrap)
