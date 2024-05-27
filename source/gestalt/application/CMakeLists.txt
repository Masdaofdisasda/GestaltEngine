message(STATUS "Configuring application module...")

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${APPLICATION_MODULE} ${HEADER_FILES} ${SOURCE_FILES})

target_compile_features(${APPLICATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${APPLICATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${APPLICATION_MODULE} PUBLIC ${FOUNDATION_MODULE})
