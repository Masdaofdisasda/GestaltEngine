message(STATUS "Configuring source...")

add_subdirectory(gestalt/application)
add_subdirectory(gestalt/foundation)
add_subdirectory(gestalt/graphics)

add_executable(${APPLICATION_NAME} gestalt/main.cpp)
set_target_properties(${APPLICATION_NAME} PROPERTIES VS_GLOBAL_VcpkgEnabled false)
target_compile_features(${APPLICATION_NAME} PRIVATE cxx_std_20)

target_link_libraries(
  ${APPLICATION_NAME}
  PRIVATE SDL2-static
          ${VULKAN_LIBRARY}
          DearImGui
          ImGuiFileDialog
          ImGuizmo
          fastgltf::fastgltf
          vk-bootstrap
          meshoptimizer
          TracyClient
          fmt::fmt
          ${APPLICATION_MODULE}
          ${FOUNDATION_MODULE}
          ${GRAPHICS_MODULE}
)

find_program(GLSL_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
message(STATUS "GLSL Validator found: ${GLSL_VALIDATOR}")

set(SHADER_SOURCE_DIR "${PROJECT_SOURCE_DIR}/source/gestalt/graphics/shaders")
set(SHADER_BINARY_DIR "${CMAKE_SOURCE_DIR}/bin/shaders")

file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

file(
  GLOB_RECURSE
  GLSL_COMPILE_FILES
  "${SHADER_SOURCE_DIR}/*.frag"
  "${SHADER_SOURCE_DIR}/*.vert"
  "${SHADER_SOURCE_DIR}/*.task"
  "${SHADER_SOURCE_DIR}/*.mesh"
  "${SHADER_SOURCE_DIR}/*.comp"
)

file(
  GLOB_RECURSE
  GLSL_SOURCE_FILES
  "${SHADER_SOURCE_DIR}/*.frag"
  "${SHADER_SOURCE_DIR}/*.vert"
  "${SHADER_SOURCE_DIR}/*.task"
  "${SHADER_SOURCE_DIR}/*.mesh"
  "${SHADER_SOURCE_DIR}/*.comp"
  "${SHADER_SOURCE_DIR}/*.glsl"
)

source_group("Source" FILES ${GLSL_SOURCE_FILES})

foreach(GLSL ${GLSL_COMPILE_FILES})
  message(STATUS "Building Shader: ${GLSL}")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${SHADER_BINARY_DIR}/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL}
  )
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  Shaders
  SOURCES ${GLSL_SOURCE_FILES}
  DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${APPLICATION_NAME} Shaders)
