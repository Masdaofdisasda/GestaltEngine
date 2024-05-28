message(STATUS "Configuring ${GRAPHICS_MODULE} module...")

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(NOT SOURCE_FILES)
  message(FATAL_ERROR "No source files found for the ${GRAPHICS_MODULE} module")
endif()

add_library(${GRAPHICS_MODULE} ${HEADER_FILES} ${SOURCE_FILES})

target_compile_features(${GRAPHICS_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${GRAPHICS_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${vk-bootstrap_SOURCE_DIR}/src"
         "${tracy_SOURCE_DIR}/public"
)

target_link_libraries(${GRAPHICS_MODULE} PUBLIC ${APPLICATION_MODULE} ${FOUNDATION_MODULE})
