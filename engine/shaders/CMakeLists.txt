message(STATUS "Configuring Shaders...")

# ───────────── Shader compilation pipeline ──────────────────────
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/engine/shaders)
set(SHADER_BIN_DIR ${CMAKE_SOURCE_DIR}/bin/shaders)

file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

if(CMAKE_CONFIGURATION_TYPES)
  # Multi-config generator (Visual Studio, Xcode, etc.)
  set(IS_MULTI_CONFIG TRUE)
else()
  set(IS_MULTI_CONFIG FALSE)
endif()

# ----------------------------------------------------------------
# Prefer CPM-built glslangValidator if available
if(TARGET glslang-standalone)
  set(GLSL_VALIDATOR "$<TARGET_FILE:glslang-standalone>")
elseif(DEFINED ENV{VULKAN_SDK})
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator")
elseif(NOT GLSL_VALIDATOR)
  find_program(GLSL_VALIDATOR glslangValidator REQUIRED)
endif()

message(STATUS "glslangValidator: ${GLSL_VALIDATOR}")


# Optional spirv-opt for Release optimisation
find_program(SPIRV_OPT spirv-opt)

# ----------------------------------------------------------------
# 1. Collect source files
file(
  GLOB_RECURSE
  GLSL_SHADER_FILES
  CONFIGURE_DEPENDS
  "${SHADER_SRC_DIR}/*.frag"
  "${SHADER_SRC_DIR}/*.vert"
  "${SHADER_SRC_DIR}/*.task"
  "${SHADER_SRC_DIR}/*.mesh"
  "${SHADER_SRC_DIR}/*.comp")

file(GLOB_RECURSE GLSL_LIBRARY_FILES CONFIGURE_DEPENDS
     "${SHADER_SRC_DIR}/*.glsl")

set(GLSL_ALL_FILES ${GLSL_SHADER_FILES} ${GLSL_LIBRARY_FILES})
source_group(
  TREE "${SHADER_SRC_DIR}"
  PREFIX "Shaders"
  FILES ${GLSL_ALL_FILES})

# ----------------------------------------------------------------
# 1. Build rules per shader
set(SPIRV_FILES "")
foreach(GLSL ${GLSL_SHADER_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${SHADER_BIN_DIR}/${FILE_NAME}.spv")

  # Ensure output folder exists
  file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

  # Debug-only flags
  set(GLSLANG_FLAGS "$<IF:$<CONFIG:Debug>,-gVS,-Os>")

  # Custom command: GLSL → SPIR-V
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V --target-env vulkan1.3 ${GLSLANG_FLAGS} ${GLSL}
            -o ${SPIRV}
    DEPENDS ${GLSL}
    COMMENT "Compiling shader  ${GLSL}"
    VERBATIM)

  # Optional optimisation pass in Release if spirv-opt is present
  if(SPIRV_OPT)
    set(OPT_SPV "${SPIRV}.opt")
    add_custom_command(
      OUTPUT ${OPT_SPV}
      COMMAND ${SPIRV_OPT} --O ${SPIRV} -o ${OPT_SPV}
      DEPENDS ${SPIRV}
      COMMENT "Optimising shader ${GLSL}"
      VERBATIM)
    list(APPEND SPIRV_FILES ${OPT_SPV})
  else()
    list(APPEND SPIRV_FILES ${SPIRV})
  endif()
endforeach()

# ----------------------------------------------------------------
# 2. Aggregate target “Shaders”
add_custom_target(
  Shaders ALL
  SOURCES ${GLSL_ALL_FILES}
  DEPENDS ${SPIRV_FILES})
set_target_properties(Shaders PROPERTIES FOLDER "Engine")
