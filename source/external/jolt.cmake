set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Distribution")

option(USE_STATIC_MSVC_RUNTIME_LIBRARY "Use static MSVC runtime library" OFF)

# When turning this option on, the library will be compiled using doubles for positions. This allows for much bigger worlds.
set(DOUBLE_PRECISION OFF)

# When turning this option on, the library will be compiled with debug symbols
set(GENERATE_DEBUG_SYMBOLS ON)

# When turning this option on, the library will be compiled in such a way to attempt to keep the simulation deterministic across platforms
set(CROSS_PLATFORM_DETERMINISTIC OFF)

# When turning this option on, the library will be compiled with interprocedural optimizations enabled, also known as link-time optimizations or link-time code generation.
# Note that if you turn this on you need to use SET_INTERPROCEDURAL_OPTIMIZATION() or set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON) to enable LTO specifically for your own project as well.
set(INTERPROCEDURAL_OPTIMIZATION ON)

# Number of bits to use in ObjectLayer. Can be 16 or 32.
set(OBJECT_LAYER_BITS 16)

# Set X86 processor features to use, by default the library compiles with AVX2, if everything is off it will be SSE2 compatible.
set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX ON)
set(USE_AVX2 ON)
set(USE_AVX512 OFF)
set(USE_LZCNT ON)
set(USE_TZCNT ON)
set(USE_F16C ON)
set(USE_FMADD ON)

CPMAddPackage(
  NAME JoltPhysics
  GITHUB_REPOSITORY jrouwe/JoltPhysics
  GIT_TAG v5.1.0
  SOURCE_SUBDIR Build
OPTIONS
    "DOUBLE_PRECISION OFF"
    "GENERATE_DEBUG_SYMBOLS ON"
    "OVERRIDE_CXX_FLAGS OFF"
    "CROSS_PLATFORM_DETERMINISTIC OFF"
    "INTERPROCEDURAL_OPTIMIZATION ON"
    "OBJECT_LAYER_BITS 16"
    "USE_SSE4_1 ${USE_SSE4_1}"
    "USE_SSE4_2 ${USE_SSE4_2}"
    "USE_AVX ${USE_AVX}"
    "USE_AVX2 ${USE_AVX2}"
    "USE_AVX512 ${USE_AVX512}"
    "USE_LZCNT ${USE_LZCNT}"
    "USE_TZCNT ${USE_TZCNT}"
    "USE_F16C ${USE_F16C}"
    "USE_FMADD ${USE_FMADD}"
    "DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF"
    "DEBUG_RENDERER_IN_DISTRIBUTION OFF"
    "PROFILER_IN_DEBUG_AND_RELEASE OFF"
    "PROFILER_IN_DISTRIBUTION OFF"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set MSVC-specific flags
if (MSVC)
    # Set runtime library
    if (USE_STATIC_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

if (INTERPROCEDURAL_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

set_property(TARGET Jolt PROPERTY FOLDER "External/")