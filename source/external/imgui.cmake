CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.90.4
)
file(GLOB IMGUI_SOURCES 
     ${imgui_SOURCE_DIR}/*.cpp 
     ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
     ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
 )

add_library(DearImGui ${IMGUI_SOURCES})

target_include_directories(DearImGui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_include_directories(DearImGui PRIVATE 
    "${Vulkan_INCLUDE_DIRS}"
)

target_link_libraries(DearImGui PRIVATE ${VULKAN_LIBRARY} SDL2-static)


set_property(TARGET DearImGui PROPERTY FOLDER "External/")