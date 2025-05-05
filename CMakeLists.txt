cmake_minimum_required(VERSION 3.28)
project(GestaltEngine LANGUAGES CXX)

# we want to fully control the dependencies
set(CMAKE_TOOLCHAIN_FILE
    ""
    CACHE STRING "Disable vcpkg")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── options ────────────────────────────────────────────────────
option(GESTALT_BUILD_TESTS "Build unit tests" OFF)
option(GESTALT_USE_CCACHE "Enable ccache if present" ON)
option(ENABLE_SANITIZERS "Build with LLVM/GCC sanitizers" OFF)

# ── compiler config ────────────────────────────────────────────
function(enable_engine_cxx_standard target)
  set_target_properties(
    ${target}
    PROPERTIES CXX_STANDARD 23
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
endfunction()

add_compile_definitions($<$<CONFIG:Debug>:TRACY_ENABLE>)
function(enable_engine_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /permissive- /Zc:__cplusplus /W4 /WX )
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow -Werror)
  endif()
endfunction()

if(CMAKE_CONFIGURATION_TYPES) # multi-config generator
  set(CMAKE_CONFIGURATION_TYPES
      "Debug;Release;Distribution"
      CACHE STRING "" FORCE)
endif()

if(GESTALT_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "ccache enabled")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  endif()
endif()

set(SAN_FLAGS
        -fsanitize=address
        -fsanitize=undefined
        -fno-omit-frame-pointer
)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND SAN_FLAGS -fuse-ld=lld)
endif()
function(add_sanitizers target)
  if (ENABLE_SANITIZERS)
    message(STATUS "add sanitizer to ${target}")
    target_compile_options(${target} PRIVATE ${SAN_FLAGS} -g)
    target_link_options   (${target} PRIVATE ${SAN_FLAGS})
  endif()
endfunction()


# ── third-party ────────────────────────────────────────────────
include(cmake/Deps.cmake)

# ── sub-directories ────────────────────────────────────────────
add_compile_definitions(JPH_FLOATING_POINT_EXCEPTIONS_ENABLED JPH_OBJECT_STREAM)

function(set_folder target folder)
  set_property(TARGET ${target} PROPERTY FOLDER ${folder})
endfunction()

add_subdirectory(engine/foundation)
add_subdirectory(engine/graphics)
add_subdirectory(engine/shaders)
add_subdirectory(engine/application)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) # to work in wsl with ninja
add_executable(GestaltApp engine/main.cpp)

add_sanitizers(Foundation)
add_sanitizers(Graphics)
add_sanitizers(Application)
add_sanitizers(GestaltApp)

enable_engine_cxx_standard(GestaltApp)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DISTRIBUTION
    ${CMAKE_SOURCE_DIR}/bin/distribution)
if(MSVC)
  set_target_properties(
    GestaltApp
    PROPERTIES
      VS_DEBUGGER_WORKING_DIRECTORY
      "$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>$<$<CONFIG:Distribution>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DISTRIBUTION}>"
  )
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT
                                                              GestaltApp)
endif()
add_dependencies(GestaltApp Shaders)

target_link_libraries(GestaltApp PRIVATE Gestalt_SDL2 Application Foundation
                                         Graphics)
set_folder(GestaltApp "App/")

if(GESTALT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(TARGETS GestaltApp RUNTIME DESTINATION bin)
