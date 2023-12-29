cmake_minimum_required(VERSION 3.20)

project(GestaltEngine)

include(cmake/CPM.cmake)
CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.0
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS 
      # set to yes skip cmake formatting
      "FORMAT_SKIP_CMAKE NO"
      # set to yes skip clang formatting
      "FORMAT_SKIP_CLANG NO"
      # path to exclude (optional, supports regular expressions)
      "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
      # extra arguments for cmake_format (optional)
      "CMAKE_FORMAT_EXTRA_ARGS -c /path/to/cmake-format.{yaml,json,py}"
)

find_package(Vulkan REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    add_compile_options(/experimental:module /std:c++latest)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GestaltApp)

# Include the source directory
add_subdirectory(source)