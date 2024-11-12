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
