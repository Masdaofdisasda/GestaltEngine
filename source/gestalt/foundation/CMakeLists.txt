message(STATUS "Configuring foundation module...")

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${FOUNDATION_MODULE} ${HEADER_FILES} ${SOURCE_FILES})

target_compile_features(${FOUNDATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${FOUNDATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
