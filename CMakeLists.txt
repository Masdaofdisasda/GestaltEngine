cmake_minimum_required(VERSION 3.28)

project(GestaltEngine)

message(STATUS "Configuring GestaltEngine...")

set(APPLICATION_NAME "GestaltApp")
set(GRAPHICS_MODULE "Graphics")
set(FOUNDATION_MODULE "Foundation")
set(APPLICATION_MODULE "Application")

if(NOT EXISTS ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
  file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
       ${CMAKE_BINARY_DIR}/cmake/CPM.cmake
  )
endif()
include(${CMAKE_BINARY_DIR}/cmake/CPM.cmake)

include(source/external/format.cmake)
include(source/external/stb.cmake)
include(source/external/glm.cmake)
include(source/external/vma.cmake)
include(source/external/volk.cmake)
include(source/external/fmt.cmake)
include(source/external/fastgltf.cmake)
include(source/external/meshopt.cmake)
include(source/external/tracy.cmake)
include(source/external/vulkan.cmake)
include(source/external/vkb.cmake)
include(source/external/sdl2.cmake)
include(source/external/imgui.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
  add_compile_options(/experimental:module /std:c++latest)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APPLICATION_NAME})

add_subdirectory(source)

set_property(TARGET ${APPLICATION_NAME} PROPERTY FOLDER "GestaltEngine")
set_property(TARGET ${GRAPHICS_MODULE} PROPERTY FOLDER "GestaltEngine")
set_property(TARGET ${FOUNDATION_MODULE} PROPERTY FOLDER "GestaltEngine")
set_property(TARGET ${APPLICATION_MODULE} PROPERTY FOLDER "GestaltEngine")
set_property(TARGET Shaders PROPERTY FOLDER "GestaltEngine")

set_target_properties(
  ${APPLICATION_NAME}
  PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY
    "$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>"
)
