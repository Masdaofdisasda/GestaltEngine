add_subdirectory(external)
add_subdirectory(gestalt/application)
add_subdirectory(gestalt/foundation)
add_subdirectory(gestalt/graphics)

add_executable(GestaltApp gestalt/main.cpp)

# Specify C++20, this is required for modules
target_compile_features(GestaltApp PRIVATE cxx_std_20)

target_link_libraries(graphics PRIVATE fmt::fmt)

# Link with other modules/libraries
target_link_libraries(GestaltApp PRIVATE SDL2::SDL2 Vulkan::Vulkan application foundation graphics)

# Find the glslangValidator program
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

# Specify the directory for shader source files and compiled shader files
set(SHADER_SOURCE_DIR "${PROJECT_SOURCE_DIR}/source/gestalt/graphics/shaders")
set(SHADER_BINARY_DIR "${CMAKE_SOURCE_DIR}/bin/shaders")

# Create the directory for compiled shader files if it does not exist
file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

# Find all shader files
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.comp"
)

# Compile each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "Building Shader: ${GLSL}")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${SHADER_BINARY_DIR}/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

# Add custom target for shaders
add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(GestaltApp Shaders)
