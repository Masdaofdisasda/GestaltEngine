message(STATUS "Configuring graphics module...")

file(GLOB_RECURSE HEADER_FILES 
     "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE SOURCE_FILES 
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(graphics
    ${HEADER_FILES}
    ${SOURCE_FILES})

target_compile_features(graphics PRIVATE cxx_std_20)

target_include_directories(graphics PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${Vulkan_INCLUDE_DIRS}
${Stb_SOURCE_DIR}
${vk-bootstrap_SOURCE_DIR}
${fmt_SOURCE_DIR}
)

