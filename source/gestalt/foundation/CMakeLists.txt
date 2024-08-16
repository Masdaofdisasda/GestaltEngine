message(STATUS "Configuring ${FOUNDATION_MODULE} module...")

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/Components/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Components/*.cpp"
)

file(GLOB_RECURSE CAMERA
    "${CMAKE_CURRENT_SOURCE_DIR}/Camera/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Camera/*.cpp"
)

file(GLOB_RECURSE MESH
    "${CMAKE_CURRENT_SOURCE_DIR}/Mesh/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Mesh/*.cpp"
)

file(GLOB_RECURSE LIGHT
    "${CMAKE_CURRENT_SOURCE_DIR}/Light/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Light/*.cpp"
)

file(GLOB_RECURSE MATERIAL
    "${CMAKE_CURRENT_SOURCE_DIR}/Material/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Material/*.cpp"
)

file(GLOB_RECURSE RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.cpp"
)

file(GLOB_RECURSE BUFFER
    "${CMAKE_CURRENT_SOURCE_DIR}/Buffer/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Buffer/*.cpp"
)

file(GLOB_RECURSE DESCRIPTOR
    "${CMAKE_CURRENT_SOURCE_DIR}/Descriptor/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Descriptor/*.cpp"
)

file(GLOB_RECURSE INTERFACES
    "${CMAKE_CURRENT_SOURCE_DIR}/Interface/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Interface/*.cpp"
)

add_library(${FOUNDATION_MODULE} ${SOURCES} ${COMPONENTS} ${CAMERA} ${MESH} ${LIGHT} ${MATERIAL} ${RESOURCES} ${BUFFER} ${DESCRIPTOR} ${INTERFACES})

source_group("Source Files" FILES ${SOURCES})
source_group("Components" FILES ${COMPONENTS})
source_group("Camera" FILES ${CAMERA})
source_group("Mesh" FILES ${MESH})
source_group("Light" FILES ${LIGHT})
source_group("Material" FILES ${MATERIAL})
source_group("GPU Resources" FILES ${RESOURCES})
source_group("Buffer" FILES ${BUFFER})
source_group("Descriptor" FILES ${DESCRIPTOR})
source_group("Interface" FILES ${INTERFACES})

target_compile_features(${FOUNDATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${FOUNDATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${Vulkan_INCLUDE_DIRS}"
         "${volk_SOURCE_DIR}"
         "${glm_SOURCE_DIR}"
         "${fmt_SOURCE_DIR}/include"
)

target_link_libraries(${FOUNDATION_MODULE} PUBLIC volk)