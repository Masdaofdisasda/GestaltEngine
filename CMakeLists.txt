cmake_minimum_required(VERSION 3.20)

project(GestaltEngine)

message(STATUS "Configuring GestaltEngine...")

set(APPLICATION_NAME "GestaltApp")

include(cmake/CPM.cmake)
CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.0
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS 
      "FORMAT_SKIP_CMAKE YES"
      "FORMAT_SKIP_CLANG NO"
      "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
)

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found: INCLUDE_DIRS=${Vulkan_INCLUDE_DIRS}, LIBRARIES=${Vulkan_LIBRARIES}")

CPMAddPackage(
  NAME SDL2
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-2.0.14  # Replace with the desired version
)
if(SDL2_ADDED)
  add_library(SDL2::SDL2 ALIAS SDL2)
endif()

CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
)

CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  VERSION 1.79
  DOWNLOAD_ONLY YES # Prevent CPM from trying to configure imgui as it doesn't have a CMakeLists.txt
)
if(imgui_ADDED)
  # Add imgui as an interface library
  add_library(imgui INTERFACE)
  target_include_directories(imgui INTERFACE ${imgui_SOURCE_DIR})
  # Specify source files to be compiled under the imgui target
  target_sources(imgui INTERFACE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    # Add other imgui source files you need
  )
  # Alias for the target (optional)
  add_library(imgui::imgui ALIAS imgui)
endif()


CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master
  DOWNLOAD_ONLY YES
)

if(stb_ADDED)
  add_library(stb INTERFACE)
  target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

CPMAddPackage(
  NAME fastgltf
  GITHUB_REPOSITORY spnda/fastgltf
  VERSION 0.6.1
  # Additional configuration may be needed depending on the package
)

CPMAddPackage(
  NAME vk-bootstrap
  GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
  GIT_TAG v1.3.274
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Check if we are on macOS
if(APPLE)
    # Specific settings for macOS/Clang
    message(STATUS "Configuring for macOS/Clang")
    # You can add any macOS/Clang specific compile options here
endif()

if(MSVC)
    add_compile_options(/experimental:module /std:c++latest)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APPLICATION_NAME})

add_subdirectory(source)

set_target_properties(${APPLICATION_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>"
)
