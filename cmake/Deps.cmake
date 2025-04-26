if(NOT EXISTS ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
  file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
       ${CMAKE_BINARY_DIR}/cmake/CPM.cmake
  )
endif()
include(${CMAKE_BINARY_DIR}/cmake/CPM.cmake)

# ------------ format ------------------------------
CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.0
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS "FORMAT_SKIP_CMAKE YES" "FORMAT_SKIP_CLANG NO" "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
)

set_property(TARGET check-clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET check-cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET check-format PROPERTY FOLDER "Formatting")
set_property(TARGET clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-format PROPERTY FOLDER "Formatting")
set_property(TARGET format PROPERTY FOLDER "Formatting")


file(GLOB_RECURSE ALL_SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/source/gestalt/*.cpp
    ${CMAKE_SOURCE_DIR}/source/gestalt/*.h
    ${CMAKE_SOURCE_DIR}/source/gestalt/graphics/*.cpp
    ${CMAKE_SOURCE_DIR}/source/gestalt/graphics/*.h
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Custom target to run clang-tidy on all source files
add_custom_target(
    clang-tidy
    COMMAND clang-tidy
    ${ALL_SOURCE_FILES}
    -p ${CMAKE_BINARY_DIR} 
    -- 
    # Add any additional clang-tidy options here, if needed.
)

add_custom_target(
    fix-clang-tidy
    COMMAND clang-tidy
    -p ${CMAKE_BINARY_DIR}
    --fix-errors 
    ${ALL_SOURCE_FILES}
    --
    # After '--', you can add additional arguments to pass to the compiler if needed
)

set_property(TARGET clang-tidy PROPERTY FOLDER "Linting")
set_property(TARGET fix-clang-tidy PROPERTY FOLDER "Linting")

# ------------ nlohmann_json ------------------------------
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
    OPTIONS
        "JSON_BuildTests OFF"
)
set_property(TARGET nlohmann_json PROPERTY FOLDER "External/")

# ------------ stb ------------------------------
CPMAddPackage(
  NAME Stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master
)

# ------------ glm ------------------------------
CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.1
  OPTIONS "GLM_BUILD_TESTS OFF"
)
set_target_properties(glm PROPERTIES VS_GLOBAL_VcpkgEnabled false)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

set_property(TARGET glm PROPERTY FOLDER "External/")

# ------------ vma ------------------------------
CPMAddPackage(
  NAME VMA
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.1.0
)
set_target_properties(VulkanMemoryAllocator PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET VulkanMemoryAllocator PROPERTY FOLDER "External/")

# ------------ volk ------------------------------
CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG  1.3.270
)
set_target_properties(volk PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET volk PROPERTY FOLDER "External/")

# ------------ fmt ------------------------------
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
  OPTIONS "FMT_TEST OFF" "FMT_DOC OFF"
)
target_compile_definitions(fmt INTERFACE FMT_SHARED=1)

set_property(TARGET fmt PROPERTY FOLDER "External/")

# ------------ fastgltf ------------------------------
CPMAddPackage(
  NAME fastgltf
  GITHUB_REPOSITORY spnda/fastgltf
  GIT_TAG v0.7.1
  OPTIONS "FASTGLTF_ENABLE_TESTS NO" "FASTGLTF_ENABLE_DOCS NO" "FASTGLTF_COMPILE_AS_CPP20 YES"
)

set_property(TARGET fastgltf PROPERTY FOLDER "External/")

# ------------ meshopt ------------------------------
CPMAddPackage(
  NAME meshoptimizer
  GITHUB_REPOSITORY zeux/meshoptimizer
  VERSION 0.20
)
set_target_properties(meshoptimizer PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET meshoptimizer PROPERTY FOLDER "External/")

# ------------ tracy ------------------------------
CPMAddPackage(
  NAME tracy
  GITHUB_REPOSITORY wolfpld/tracy
  VERSION 0.10
)
add_compile_definitions(TRACY_ENABLE)

set_target_properties(TracyClient PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET TracyClient PROPERTY FOLDER "External/")

# ------------ Vulkan ------------------------------
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

# ------------ vk-bootstrap ------------------------------
CPMAddPackage(
  NAME vk-bootstrap
  GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
  GIT_TAG v1.3.280
  OPTIONS "DVK_BOOTSTRAP_TEST OFF" "DVK_BOOTSTRAP_INSTALL ON"
          "DVK_BOOTSTRAP_VULKAN_HEADER_DIR ${Vulkan_INCLUDE_DIRS}"
)
set_target_properties(vk-bootstrap PROPERTIES VS_GLOBAL_VcpkgEnabled false)

target_include_directories(vk-bootstrap PRIVATE "${Vulkan_INCLUDE_DIRS}")

set_property(TARGET vk-bootstrap PROPERTY FOLDER "External/")

# ------------ SDL2 ------------------------------
CPMAddPackage(
  NAME SDL2
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-2.30.1
  OPTIONS "SDL2_DISABLE_INSTALL ON" "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_STATIC_PIC ON"
          "SDL_WERROR OFF"
)
file(GLOB SDL2_HEADERS "${SDL2_SOURCE_DIR}/include/*.h")

add_custom_target(
  sdl_copy_headers_in_build_dir
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${SDL2_SOURCE_DIR}/include"
          "${CMAKE_BINARY_DIR}/SDLHeaders/SDL2"
  DEPENDS ${SDL2_HEADERS}
)

add_dependencies(SDL2-static sdl_copy_headers_in_build_dir)
target_include_directories(SDL2-static INTERFACE "${CMAKE_BINARY_DIR}/SDLHeaders")
# Define SDL_MAIN_HANDLED globally for SDL2-static
target_compile_definitions(SDL2-static INTERFACE SDL_MAIN_HANDLED)

set(SDL2_INCLUDE_DIR ${SDL2_SOURCE_DIR}/include)
include_directories(${SDL2_INCLUDE_DIR})

set_property(TARGET SDL2-static PROPERTY FOLDER "External/SDL2")
set_property(TARGET sdl_copy_headers_in_build_dir PROPERTY FOLDER "External/SDL2")
set_property(TARGET sdl_headers_copy PROPERTY FOLDER "External/SDL2")
set_property(TARGET SDL2_test PROPERTY FOLDER "External/SDL2")
set_property(TARGET SDL2main PROPERTY FOLDER "External/SDL2")

# ------------ soloud ------------------------------
CPMAddPackage(
  NAME SoLoud
  GITHUB_REPOSITORY jarikomppa/soloud
  GIT_TAG RELEASE_20200207
  SOURCE_SUBDIR contrib
  OPTIONS "WITH_SDL2_STATIC ON" # Use SDL2 for audio backend
)

set_property(TARGET soloud PROPERTY FOLDER "External")

# ------------ imgui ------------------------------
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

target_compile_definitions(DearImGui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES VK_NO_PROTOTYPES)

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
  COMMAND python ${CMAKE_SOURCE_DIR}/cmake/patch_ImGuizmo.py
          ${ImGuizmo_SOURCE_DIR}/GraphEditor.cpp
  COMMENT "Applying patches to ImGuizmo/GraphEditor.cpp"
)

target_compile_definitions(ImGuizmo PRIVATE IMGUI_DEFINE_MATH_OPERATORS) # triggers a warning but
                                                                         # wont work otherwise
set_property(TARGET ImGuizmo PROPERTY FOLDER "External/ImGUI")

# ------------ jolt ------------------------------

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(GENERATE_DEBUG_SYMBOLS ON)
else()
  set(GENERATE_DEBUG_SYMBOLS OFF)
endif()

set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use static runtime library" FORCE)

CPMAddPackage(
  NAME JoltPhysics
  GITHUB_REPOSITORY jrouwe/JoltPhysics
  GIT_TAG v5.1.0
  SOURCE_SUBDIR Build
  OPTIONS
        "DOUBLE_PRECISION OFF"
        "GENERATE_DEBUG_SYMBOLS ${GENERATE_DEBUG_SYMBOLS}"
        "OVERRIDE_CXX_FLAGS OFF"
        "CROSS_PLATFORM_DETERMINISTIC OFF"
        "INTERPROCEDURAL_OPTIMIZATION OFF"
        "OBJECT_LAYER_BITS 16"
        "USE_SSE4_1 ON"
        "USE_SSE4_2 ON"
        "USE_AVX ON"
        "USE_AVX2 ON"
        "USE_AVX512 OFF"
        "USE_LZCNT ON"
        "USE_TZCNT ON"
        "USE_F16C ON"
        "USE_FMADD ON"
        "DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF"
        "DEBUG_RENDERER_IN_DISTRIBUTION OFF"
        "PROFILER_IN_DEBUG_AND_RELEASE OFF"
        "PROFILER_IN_DISTRIBUTION OFF"
        "ENABLE_ALL_WARNINGS OFF"
)

set_property(TARGET Jolt PROPERTY FOLDER "External/")
