cmake_minimum_required(VERSION 3.28)
project(GestaltEngine LANGUAGES CXX)

# ── options ────────────────────────────────────────────────────
option(GESTALT_BUILD_TESTS  "Build unit tests"         OFF)
option(GESTALT_USE_CCACHE   "Enable ccache if present" ON)

# ── compiler config ────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions($<$<CONFIG:Debug>:TRACY_ENABLE>)
if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus)
	add_compile_options(/experimental:module /std:c++latest)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    add_compile_options(-Wall -Wextra -Wshadow -Werror)
endif()
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Distribution" CACHE STRING "Supported build types" FORCE)

if(GESTALT_USE_CCACHE AND NOT MSVC)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "Using ccache")
        set(CMAKE_C_COMPILER_LAUNCHER   ${CCACHE_PROGRAM})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    endif()
endif()

# ── third-party ────────────────────────────────────────────────
include(cmake/Deps.cmake)

# ── sub-directories ────────────────────────────────────────────
add_compile_definitions(
        JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
        JPH_OBJECT_STREAM
)

function(set_folder target folder)
    set_property(TARGET ${target} PROPERTY FOLDER ${folder})
endfunction()

add_subdirectory(engine/foundation)
add_subdirectory(engine/graphics)
add_subdirectory(engine/shaders)
add_subdirectory(engine/application)
add_executable(GestaltApp engine/main.cpp)
if (MSVC)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/debug")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DISTRIBUTION "${CMAKE_SOURCE_DIR}/bin/distribution")
	set_target_properties(GestaltApp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>$<$<CONFIG:Distribution>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DISTRIBUTION}>")
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GestaltApp)
endif()
set_target_properties(Foundation PROPERTIES VS_GLOBAL_VcpkgEnabled false)
set_target_properties(Application PROPERTIES VS_GLOBAL_VcpkgEnabled false)
set_target_properties(Graphics PROPERTIES VS_GLOBAL_VcpkgEnabled false)
set_target_properties(GestaltApp PROPERTIES VS_GLOBAL_VcpkgEnabled false)
add_dependencies(GestaltApp Shaders)
	
target_link_libraries(
  GestaltApp
  PRIVATE ${VULKAN_LIBRARY}
          DearImGui
          ImGuiFileDialog
          ImGuizmo
          fastgltf::fastgltf
          vk-bootstrap
          meshoptimizer
          TracyClient
          fmt::fmt
		  Jolt
		  soloud
          Application
          Foundation
          Graphics
)
set_folder(GestaltApp "App")

if(GESTALT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
