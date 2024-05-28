message(STATUS "Configuring ${FOUNDATION_MODULE} module...")

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(NOT SOURCE_FILES)
  message(FATAL_ERROR "No source files found for the ${FOUNDATION_MODULE} module")
endif()

add_library(${FOUNDATION_MODULE} ${HEADER_FILES} ${SOURCE_FILES})

target_compile_features(${FOUNDATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${FOUNDATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${Vulkan_INCLUDE_DIRS}"
         "${glm_SOURCE_DIR}"
         "${fmt_SOURCE_DIR}/include"
)