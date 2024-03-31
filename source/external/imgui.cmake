CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.90.4
)
file(GLOB IMGUI_SOURCES ${imgui_SOURCE_DIR}/*.cpp
     ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
     ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
)

add_library(DearImGui ${IMGUI_SOURCES})

target_include_directories(DearImGui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

target_link_libraries(DearImGui PRIVATE ${VULKAN_LIBRARY} SDL2-static)

target_include_directories(DearImGui PRIVATE "${Vulkan_INCLUDE_DIRS}")

set_property(TARGET DearImGui PROPERTY FOLDER "External/ImGUI")

# Add ImGuiNodeEditor CPMAddPackage( NAME ImGuiNodeEditor GITHUB_REPOSITORY thedmd/imgui-node-editor
# GIT_TAG v0.9.3 DOWNLOAD_ONLY YES )

# Add ImPlot CPMAddPackage( NAME implot GITHUB_REPOSITORY epezent/implot GIT_TAG v0.13 # Specify a
# specific commit or tag if needed )

# Add ImGuiFileDialog
CPMAddPackage(
  NAME ImGuiFileDialog
  GITHUB_REPOSITORY aiekick/ImGuiFileDialog
  GIT_TAG v0.6.7
)

target_include_directories(ImGuiFileDialog PRIVATE "${imgui_SOURCE_DIR}")
set_property(TARGET ImGuiFileDialog PROPERTY FOLDER "External/ImGUI")

# Add ImGuizmo TODO needs older ImGui version
CPMAddPackage(
  NAME ImGuizmo
  GITHUB_REPOSITORY CedricGuillemet/ImGuizmo
  GIT_TAG 1.83
)

file(GLOB IMGUIZMO_SOURCES ${ImGuizmo_SOURCE_DIR}/*.cpp ${ImGuizmo_SOURCE_DIR}/*.h)

add_library(ImGuizmo STATIC ${IMGUIZMO_SOURCES})
target_include_directories(ImGuizmo PRIVATE "${imgui_SOURCE_DIR}")
add_custom_command(
  TARGET ImGuizmo
  PRE_BUILD
  COMMAND python ${CMAKE_SOURCE_DIR}/source/external/patch_ImGuizmo.py
          ${ImGuizmo_SOURCE_DIR}/GraphEditor.cpp
  COMMENT "Applying patches to ImGuizmo/GraphEditor.cpp"
)

target_compile_definitions(ImGuizmo PRIVATE IMGUI_DEFINE_MATH_OPERATORS) # triggers a warning but
                                                                         # wont work otherwise
set_property(TARGET ImGuizmo PROPERTY FOLDER "External/ImGUI")
