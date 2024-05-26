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
#add_library(imgui::imgui ALIAS DearImGui) # required for ImNodeFlow

target_include_directories(DearImGui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

target_link_libraries(DearImGui PRIVATE ${VULKAN_LIBRARY} SDL2-static)

target_include_directories(DearImGui PRIVATE "${Vulkan_INCLUDE_DIRS}")

set_property(TARGET DearImGui PROPERTY FOLDER "External/ImGUI")

# Add ImNodeFlow
#CPMAddPackage(
#  NAME ImNodeFlow
#  GITHUB_REPOSITORY Fattorino/ImNodeFlow
#  GIT_TAG v1.2.1
#  SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/includes/ImNodeFlow"
#)

#target_include_directories(ImNodeFlow PRIVATE "${imgui_SOURCE_DIR}")
#set_property(TARGET ImNodeFlow PROPERTY FOLDER "External/ImGUI")

# Add ImGuiFileDialog
CPMAddPackage(
  NAME ImGuiFileDialog
  GITHUB_REPOSITORY aiekick/ImGuiFileDialog
  GIT_TAG v0.6.7
)

target_include_directories(ImGuiFileDialog PRIVATE "${imgui_SOURCE_DIR}")
set_property(TARGET ImGuiFileDialog PROPERTY FOLDER "External/ImGUI")

# Add ImGuizmo
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
