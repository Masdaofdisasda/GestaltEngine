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
  
  OPTIONS
		"DVK_BOOTSTRAP_TEST OFF"
        "DVK_BOOTSTRAP_INSTALL ON"
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

CPMAddPackage(
  NAME fastgltf
  GITHUB_REPOSITORY spnda/fastgltf
  GIT_TAG v0.7.1
  OPTIONS 
		"FASTGLTF_ENABLE_TESTS NO"
		"FASTGLTF_ENABLE_DOCS NO"
		"FASTGLTF_COMPILE_AS_CPP20 YES"
)

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found: INCLUDE_DIRS=${Vulkan_INCLUDE_DIRS}, LIBRARIES=${Vulkan_LIBRARIES}")

CPMAddPackage(
  NAME SDL2
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-2.30.1
	OPTIONS
        "SDL2_DISABLE_INSTALL ON"
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_STATIC_PIC ON"
        "SDL_WERROR OFF"
)
file(GLOB SDL2_HEADERS "${SDL2_SOURCE_DIR}/include/*.h")

add_custom_target(sdl_copy_headers_in_build_dir
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${SDL2_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/SDLHeaders/SDL2"
        DEPENDS ${SDL2_HEADERS})
		
add_dependencies(SDL2-static sdl_copy_headers_in_build_dir)
target_include_directories(SDL2-static INTERFACE "${CMAKE_BINARY_DIR}/SDLHeaders")

set (SDL2_INCLUDE_DIR ${SDL2_SOURCE_DIR}/include)
include_directories(${SDL2_INCLUDE_DIR})


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

# Create a library or executable with ImGui
add_library(DearImGui ${IMGUI_SOURCES})

target_include_directories(DearImGui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(DearImGui PRIVATE Vulkan::Vulkan SDL2-static)

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