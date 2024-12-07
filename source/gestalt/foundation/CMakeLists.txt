message(STATUS "Configuring ${FOUNDATION_MODULE} module...")

# automate file collection and source grouping
function(add_sources_from_folder folder_name)
    file(GLOB_RECURSE SOURCES_IN_FOLDER
        "${CMAKE_CURRENT_SOURCE_DIR}/${folder_name}/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${folder_name}/*.cpp"
    )

    list(APPEND ALL_SOURCES ${SOURCES_IN_FOLDER})

    string(REPLACE "/" "\\" group_name "${folder_name}")
    source_group("${group_name}" FILES ${SOURCES_IN_FOLDER})
endfunction()

# NOTE: source code will be added from these folders:
set(FOLDERS "Components" "Camera" "Mesh" "Light" "Material" "Resources" "Buffer" "Animation" "Descriptor" "Interface")

foreach(folder ${FOLDERS})
    add_sources_from_folder(${folder})
endforeach()

# Add main source files from root directory
file(GLOB_RECURSE ROOT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(APPEND ALL_SOURCES ${ROOT_SOURCES})
source_group("Source Files" FILES ${ROOT_SOURCES})

add_library(${FOUNDATION_MODULE} ${ALL_SOURCES})

target_compile_features(${FOUNDATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${FOUNDATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${Vulkan_INCLUDE_DIRS}"
         "${VMA_SOURCE_DIR}/include"
         "${volk_SOURCE_DIR}"
         "${glm_SOURCE_DIR}"
         "${fmt_SOURCE_DIR}/include"
         "${nlohmann_json_SOURCE_DIR}/include"
)

target_link_libraries(${FOUNDATION_MODULE} PUBLIC volk)