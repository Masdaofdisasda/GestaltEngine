cmake_minimum_required(VERSION 3.28)

project(GestaltEngine)

message(STATUS "Configuring GestaltEngine...")

set(APPLICATION_NAME "GestaltApp")

if(NOT EXISTS ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
  file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
endif()
include(${CMAKE_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.0
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS 
      "FORMAT_SKIP_CMAKE YES"
      "FORMAT_SKIP_CLANG NO"
      "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
)
CPMAddPackage(
  NAME Stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master 
)

CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.1
)

CPMAddPackage(
  NAME VMA
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.0.1
)

CPMAddPackage(
  NAME vk-bootstrap
  GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
  GIT_TAG v1.3.274
)

CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
  
  OPTIONS
      "FMT_TEST OFF"
      "FMT_DOC OFF"
)

target_compile_definitions(fmt INTERFACE FMT_SHARED=1)

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found: INCLUDE_DIRS=${Vulkan_INCLUDE_DIRS}, LIBRARIES=${Vulkan_LIBRARIES}")
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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