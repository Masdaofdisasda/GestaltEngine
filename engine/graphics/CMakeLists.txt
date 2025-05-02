message(STATUS "Configuring Graphics module...")

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
set(FOLDERS "Renderpasses" "Utils" "Render Engine")

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

add_library(Graphics ${ALL_SOURCES})
enable_engine_cxx_standard(Graphics)
#enable_engine_warnings(Graphics)

target_include_directories(
  Graphics
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${vk-bootstrap_SOURCE_DIR}/src"
         "${tracy_SOURCE_DIR}/public"
)

target_link_libraries(Graphics PRIVATE Gestalt_SDL2 Tracy::TracyClient vk-bootstrap fmt::fmt Application Foundation)
set_folder(Graphics "Engine/")