if(NOT EXISTS ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
  file(DOWNLOAD
       https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
       ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
endif()
include(${CMAKE_BINARY_DIR}/cmake/CPM.cmake)

# ------------ format ------------------------------
cpmaddpackage(
  NAME
  Format.cmake
  VERSION
  1.8.0
  GITHUB_REPOSITORY
  TheLartians/Format.cmake
  OPTIONS
  "FORMAT_SKIP_CMAKE YES"
  "FORMAT_SKIP_CLANG NO"
  "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake")

set_property(TARGET check-clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET check-cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET check-format PROPERTY FOLDER "Formatting")
set_property(TARGET clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-format PROPERTY FOLDER "Formatting")
set_property(TARGET format PROPERTY FOLDER "Formatting")

file(
  GLOB_RECURSE
  ALL_SOURCE_FILES
  ${CMAKE_SOURCE_DIR}/source/gestalt/*.cpp
  ${CMAKE_SOURCE_DIR}/source/gestalt/*.h
  ${CMAKE_SOURCE_DIR}/source/gestalt/graphics/*.cpp
  ${CMAKE_SOURCE_DIR}/source/gestalt/graphics/*.h)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Custom target to run clang-tidy on all source files
add_custom_target(
  clang-tidy COMMAND clang-tidy ${ALL_SOURCE_FILES} -p ${CMAKE_BINARY_DIR} --
  # Add any additional clang-tidy options here, if needed.
)

add_custom_target(
  fix-clang-tidy
  COMMAND clang-tidy -p ${CMAKE_BINARY_DIR} --fix-errors ${ALL_SOURCE_FILES} --
  # After '--', you can add additional arguments to pass to the compiler if
  # needed
)

set_property(TARGET clang-tidy PROPERTY FOLDER "Linting")
set_property(TARGET fix-clang-tidy PROPERTY FOLDER "Linting")

# ------------ nlohmann_json ------------------------------
cpmaddpackage(
  NAME
  nlohmann_json
  GITHUB_REPOSITORY
  nlohmann/json
  VERSION
  3.11.3
  OPTIONS
  "JSON_BuildTests OFF")
set_property(TARGET nlohmann_json PROPERTY FOLDER "External/")

# ------------ stb ------------------------------
cpmaddpackage(NAME Stb GITHUB_REPOSITORY nothings/stb GIT_TAG master)
add_library(Gestalt_Stb INTERFACE)

target_include_directories(Gestalt_Stb INTERFACE ${Stb_SOURCE_DIR})

# ------------ glm ------------------------------
cpmaddpackage(
  NAME
  glm
  GITHUB_REPOSITORY
  g-truc/glm
  GIT_TAG
  1.0.1
  OPTIONS
  "GLM_BUILD_TESTS OFF")
set_target_properties(glm PROPERTIES VS_GLOBAL_VcpkgEnabled false)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

set_property(TARGET glm PROPERTY FOLDER "External/")

# ------------ vma ------------------------------
cpmaddpackage(NAME VMA GITHUB_REPOSITORY
              GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator GIT_TAG v3.1.0)
set_target_properties(VulkanMemoryAllocator PROPERTIES VS_GLOBAL_VcpkgEnabled
                                                       false)
target_include_directories(VulkanMemoryAllocator
                           INTERFACE "${VMA_SOURCE_DIR}/include")

set_property(TARGET VulkanMemoryAllocator PROPERTY FOLDER "External/")

# ------------ fmt ------------------------------
cpmaddpackage(
  NAME
  fmt
  GITHUB_REPOSITORY
  fmtlib/fmt
  GIT_TAG
  10.2.1
  OPTIONS
  "FMT_TEST OFF"
  "FMT_DOC OFF"
  "FMT_SHARED OFF")

set_property(TARGET fmt PROPERTY FOLDER "External/")

# ------------ fastgltf ------------------------------
cpmaddpackage(
  NAME
  fastgltf
  GITHUB_REPOSITORY
  spnda/fastgltf
  GIT_TAG
  v0.7.1
  OPTIONS
  "FASTGLTF_ENABLE_TESTS NO"
  "FASTGLTF_ENABLE_DOCS NO"
  "FASTGLTF_COMPILE_AS_CPP20 YES")

set_property(TARGET fastgltf PROPERTY FOLDER "External/")

# ------------ meshopt ------------------------------
cpmaddpackage(NAME meshoptimizer GITHUB_REPOSITORY zeux/meshoptimizer VERSION
              0.20)
set_target_properties(meshoptimizer PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET meshoptimizer PROPERTY FOLDER "External/")

# ------------ tracy ------------------------------
cpmaddpackage(NAME tracy GITHUB_REPOSITORY wolfpld/tracy VERSION 0.10)
add_compile_definitions(TRACY_ENABLE)

set_target_properties(TracyClient PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET TracyClient PROPERTY FOLDER "External/")
# -------- Vulkan Headers ---------------------------
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
CPMAddPackage(
  NAME VulkanHeaders
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
  GIT_TAG vulkan-sdk-1.3.275.0
)

set(Vulkan_INCLUDE_DIRS "${VulkanHeaders_SOURCE_DIR}/include")

# -------- Vulkan Loader ----------------------------
CPMAddPackage(
  NAME VulkanLoader
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Loader
  GIT_TAG vulkan-sdk-1.3.275.0
  OPTIONS
    "BUILD_TESTS OFF"
    "VULKAN_HEADERS_INSTALL OFF"
)

set_target_properties(vulkan PROPERTIES FOLDER "External/Vulkan")
set_target_properties(asm_offset PROPERTIES FOLDER "External/Vulkan")
if (TARGET loader-opt)
  set_target_properties(loader-opt PROPERTIES FOLDER "External/Vulkan")
endif ()
if (TARGET loader-unknown-chain)
  set_target_properties(loader-unknown-chain PROPERTIES FOLDER "External/Vulkan")
endif ()
set_target_properties(loader_asm_gen_files PROPERTIES FOLDER "External/Vulkan")

# -------- SPIRV-Headers -----------------------------
CPMAddPackage(
  NAME SPIRV-Headers
  GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
  GIT_TAG vulkan-sdk-1.3.275.0
)

# -------- SPIRV-Tools ------------------------
CPMAddPackage(
  NAME SPIRV-Tools
  GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
  GIT_TAG vulkan-sdk-1.3.275.0
  OPTIONS
    "SPIRV_SKIP_EXECUTABLES ON"
    "SPIRV-Headers_SOURCE_DIR ${SPIRV-Headers_SOURCE_DIR}"
)

set_target_properties(spirv-tools-build-version PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spirv-tools-header-DebugInfo PROPERTIES FOLDER "External/SPIRV-Tools")
set_target_properties(spirv-tools-header-NonSemanticShaderDebugInfo100 PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spirv-tools-header-OpenCLDebugInfo100 PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-cldi100 PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-clspvreflection PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-debuginfo PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-shdi100 PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-spv-amd-gs PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-spv-amd-sb PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-spv-amd-sevp PROPERTIES FOLDER "External/SPIRV-Tools/Build")
set_target_properties(spv-tools-spv-amd-stm PROPERTIES FOLDER "External/SPIRV-Tools/Build")

set_target_properties(SPIRV-Tools-diff PROPERTIES FOLDER "External/SPIRV-Tools/Library")
set_target_properties(SPIRV-Tools-link PROPERTIES FOLDER "External/SPIRV-Tools/Library")
set_target_properties(SPIRV-Tools-lint PROPERTIES FOLDER "External/SPIRV-Tools/Library")
set_target_properties(SPIRV-Tools-opt PROPERTIES FOLDER "External/SPIRV-Tools/Library")
set_target_properties(SPIRV-Tools-reduce PROPERTIES FOLDER "External/SPIRV-Tools/Library")
set_target_properties(SPIRV-Tools-shared PROPERTIES FOLDER "External/SPIRV-Tools/Library")
set_target_properties(SPIRV-Tools-static PROPERTIES FOLDER "External/SPIRV-Tools/Library")

set_target_properties(spirv-tools-vimsyntax PROPERTIES FOLDER "External/SPIRV-Tools/Utilities")

set_target_properties(spirv-tools-pkg-config PROPERTIES FOLDER "External/SPIRV-Tools")
set_target_properties(spirv-tools-shared-pkg-config PROPERTIES FOLDER "External/SPIRV-Tools")
set_target_properties(core_tables PROPERTIES FOLDER "External/SPIRV-Tools")
set_target_properties(enum_string_mapping PROPERTIES FOLDER "External/SPIRV-Tools")
set_target_properties(extinst_tables PROPERTIES FOLDER "External/SPIRV-Tools")

# -------- glslang Validator ------------------------
CPMAddPackage(
  NAME glslang
  GITHUB_REPOSITORY KhronosGroup/glslang
  GIT_TAG vulkan-sdk-1.3.275.0
  OPTIONS
    "ENABLE_CTEST OFF"
    "ENABLE_GLSLANG_BINARIES ON"
    "ENABLE_SPVREMAPPER OFF"
    "ENABLE_HLSL OFF"
)

set_target_properties(glslang PROPERTIES FOLDER "External/glslang")
set_target_properties(GenericCodeGen PROPERTIES FOLDER "External/glslang")
set_target_properties(glslang-default-resource-limits PROPERTIES FOLDER "External/glslang")
set_target_properties(OSDependent PROPERTIES FOLDER "External/glslang")
set_target_properties(MachineIndependent PROPERTIES FOLDER "External/glslang")
set_target_properties(SPIRV PROPERTIES FOLDER "External/glslang")
set_target_properties(glslang-standalone PROPERTIES FOLDER "External/glslang")

# -------- VulkanUtilityLibraries ------
CPMAddPackage(
    NAME VulkanUtilityLibraries
    GITHUB_REPOSITORY KhronosGroup/Vulkan-Utility-Libraries
    GIT_TAG vulkan-sdk-1.3.275
    OPTIONS
      "VULKAN_UTILITY_LIBRARIES_BUILD_EXAMPLES OFF"
      "VULKAN_UTILITY_LIBRARIES_BUILD_TESTS OFF"
      "VULKAN_UTILITY_LIBRARIES_BUILD_DOCUMENTATION OFF"
)

set_target_properties(VulkanLayerSettings PROPERTIES FOLDER "External/Vulkan")
set_target_properties(VulkanUtilityHeaders PROPERTIES FOLDER "External/Vulkan")

# emulate what find_package(Vulkan) would have set
set(Vulkan_LIBRARIES Vulkan::Vulkan)
set(VK_LAYER_PATH "$<TARGET_FILE_DIR:VkLayer_khronos_validation>")

# ------------ volk ------------------------------
cpmaddpackage(NAME volk GITHUB_REPOSITORY zeux/volk GIT_TAG 1.3.270)
set_target_properties(volk PROPERTIES VS_GLOBAL_VcpkgEnabled false)

target_link_libraries(volk PRIVATE Vulkan::Headers)

set_property(TARGET volk PROPERTY FOLDER "External/")
# ------------ vk-bootstrap ------------------------------
cpmaddpackage(
  NAME
  vk-bootstrap
  GITHUB_REPOSITORY
  charles-lunarg/vk-bootstrap
  GIT_TAG
  v1.3.280
  OPTIONS
  "DVK_BOOTSTRAP_TEST OFF"
  "DVK_BOOTSTRAP_INSTALL ON"
  "DVK_BOOTSTRAP_VULKAN_HEADER_DIR ${Vulkan_INCLUDE_DIRS}")
set_target_properties(vk-bootstrap PROPERTIES VS_GLOBAL_VcpkgEnabled false)

target_include_directories(vk-bootstrap PRIVATE "${Vulkan_INCLUDE_DIRS}")

set_property(TARGET vk-bootstrap PROPERTY FOLDER "External/")

# ───────────── SDL2 (static) ─────────────────────────────────────
cpmaddpackage(
  NAME
  SDL2
  GITHUB_REPOSITORY
  libsdl-org/SDL
  GIT_TAG
  release-2.30.1
  OPTIONS
  "SDL_SHARED OFF"
  "SDL_STATIC ON"
  "SDL_TEST   OFF"
  "SDL_INSTALL OFF"
  "SDL_WERROR OFF")
add_library(Gestalt_SDL2 INTERFACE)

target_link_libraries(Gestalt_SDL2 INTERFACE SDL2-static)
target_include_directories(Gestalt_SDL2 INTERFACE ${SDL2_SOURCE_DIR}/include)
target_compile_definitions(Gestalt_SDL2
                           INTERFACE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)

set_target_properties(SDL2-static PROPERTIES FOLDER "External/SDL2")
set_property(TARGET sdl_headers_copy PROPERTY FOLDER "External/SDL2")
set_property(TARGET SDL2_test PROPERTY FOLDER "External/SDL2")
set_property(TARGET SDL2main PROPERTY FOLDER "External/SDL2")

# ------------ soloud ------------------------------
cpmaddpackage(
  NAME
  SoLoud
  GITHUB_REPOSITORY
  jarikomppa/soloud
  GIT_TAG
  RELEASE_20200207
  SOURCE_SUBDIR
  contrib
  OPTIONS
  "WITH_SDL2_STATIC ON"
  "SDL2_INCLUDE_DIR ${SDL2_SOURCE_DIR}/include"
  "SDL2_LIBRARY ${SDL2_STATIC_LIBRARY_PATH}")

add_library(Gestalt_SoLoud INTERFACE)

target_link_libraries(Gestalt_SoLoud INTERFACE soloud)
target_include_directories(Gestalt_SoLoud
                           INTERFACE "${SoLoud_SOURCE_DIR}/include")

set_property(TARGET soloud PROPERTY FOLDER "External")

# ------------ imgui ------------------------------

# --------------------------------------------------------------------
# 1) Dear ImGui core
cpmaddpackage(NAME imgui GITHUB_REPOSITORY ocornut/imgui GIT_TAG v1.90.4)

# ImGui ships without CMake.  Create our own tiny target that: • compiles the
# “core” files • later we’ll append backend .cpp’s with target_sources()
add_library(
  DearImGui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp)

# Public headers for anyone who links to DearImGui
target_include_directories(DearImGui PUBLIC ${imgui_SOURCE_DIR})

set_target_properties(DearImGui PROPERTIES FOLDER "External/ImGui")

# --------------------------------------------------------------------
# 2) Back-ends (SDL2 + Vulkan)
target_sources(
  DearImGui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
                    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

# Back-end headers must also be visible to users (target_include_directories
# PUBLIC)
target_include_directories(DearImGui PUBLIC ${imgui_SOURCE_DIR}/backends)

# Link the run-time dependencies the back-ends need
target_link_libraries(DearImGui PRIVATE Vulkan::Headers Vulkan::Loader Gestalt_SDL2)

# Tell ImGui back-ends that we use volk/no prototypes
target_compile_definitions(DearImGui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES
                                             VK_NO_PROTOTYPES)

# --------------------------------------------------------------------
# 3) Plug-in: ImGuiFileDialog
cpmaddpackage(NAME ImGuiFileDialog GITHUB_REPOSITORY aiekick/ImGuiFileDialog
              GIT_TAG v0.6.7)
target_include_directories(ImGuiFileDialog PRIVATE "${imgui_SOURCE_DIR}")
set_target_properties(ImGuiFileDialog PROPERTIES FOLDER "External/ImGui")

# --------------------------------------------------------------------
# 4) Plug-in: ImGuizmo
cpmaddpackage(NAME ImGuizmo GITHUB_REPOSITORY CedricGuillemet/ImGuizmo GIT_TAG
              1.83)

# ImGuizmo is a single .cpp library – compile it
add_library(ImGuizmo STATIC ${ImGuizmo_SOURCE_DIR}/ImGuizmo.cpp
                            ${ImGuizmo_SOURCE_DIR}/GraphEditor.cpp)

target_include_directories(
  ImGuizmo PUBLIC ${ImGuizmo_SOURCE_DIR} ${imgui_SOURCE_DIR} # needs ImGui
                                                             # headers
)
find_package(
  Python3
  COMPONENTS Interpreter
  REQUIRED)

add_custom_target(
  PatchImGuizmo
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/patch_ImGuizmo.py
          ${ImGuizmo_SOURCE_DIR}/GraphEditor.cpp
  COMMENT "Applying patch to ImGuizmo/GraphEditor.cpp")

set_target_properties(PatchImGuizmo PROPERTIES FOLDER "External/ImGui")

add_dependencies(ImGuizmo PatchImGuizmo)

target_compile_definitions(
  ImGuizmo PRIVATE IMGUI_DEFINE_MATH_OPERATORS # required by ImGuizmo
)

set_target_properties(ImGuizmo PROPERTIES FOLDER "External/ImGui")

# --------------------------------------------------------------------
# 6) Convenience “bundle” target
# --------------------------------------------------------------------
# If your engine wants “all of ImGui”, depend on this one target
add_library(Gestalt_ImGui INTERFACE)
target_link_libraries(Gestalt_ImGui INTERFACE DearImGui ImGuiFileDialog
                                              ImGuizmo)
set_target_properties(Gestalt_ImGui PROPERTIES FOLDER "External/ImGui")

# ------------ jolt ------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(GENERATE_DEBUG_SYMBOLS ON)
else()
  set(GENERATE_DEBUG_SYMBOLS OFF)
endif()

set(USE_STATIC_MSVC_RUNTIME_LIBRARY
    OFF
    CACHE BOOL "Use static runtime library" FORCE)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "" FORCE)        # silence policy warning just for Jolt
set(CPP_RTTI_ENABLED ON)
cpmaddpackage(
  NAME
  JoltPhysics
  GITHUB_REPOSITORY
  jrouwe/JoltPhysics
  GIT_TAG
  v5.1.0
  SOURCE_SUBDIR
  Build
  OPTIONS
  "DOUBLE_PRECISION OFF"
  "GENERATE_DEBUG_SYMBOLS ${GENERATE_DEBUG_SYMBOLS}"
  "OVERRIDE_CXX_FLAGS OFF"
  "CROSS_PLATFORM_DETERMINISTIC OFF"
  "INTERPROCEDURAL_OPTIMIZATION OFF"
  "ENABLE_OBJECT_STREAM ON"
  "OBJECT_LAYER_BITS 16"
  "USE_SSE4_1 ON"
  "USE_SSE4_2 ON"
  "USE_AVX ON"
  "USE_AVX2 ON"
  "USE_AVX512 OFF"
  "USE_LZCNT ON"
  "USE_TZCNT ON"
  "USE_F16C ON"
  "USE_FMADD ON"
  "DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF"
  "DEBUG_RENDERER_IN_DISTRIBUTION OFF"
  "PROFILER_IN_DEBUG_AND_RELEASE OFF"
  "PROFILER_IN_DISTRIBUTION OFF"
  "ENABLE_ALL_WARNINGS OFF")

set_property(TARGET Jolt PROPERTY FOLDER "External/")
