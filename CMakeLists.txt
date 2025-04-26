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

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Distribution" CACHE STRING "Supported build types" FORCE)

include(cmake/Deps.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DISTRIBUTION "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_DISTRIBUTION "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_DISTRIBUTION "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_DISTRIBUTION "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

if(MSVC)
  add_compile_options(/experimental:module /std:c++latest)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DISTRIBUTION "${CMAKE_SOURCE_DIR}/bin/distribution")

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
    "$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>$<$<CONFIG:Distribution>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DISTRIBUTION}>"
)
