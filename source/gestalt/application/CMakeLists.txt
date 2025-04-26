message(STATUS "Configuring ${APPLICATION_MODULE} module...")

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
set(FOLDERS "ECS" "Resource Loading" "Events")

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

add_library(${APPLICATION_MODULE} ${ALL_SOURCES})
target_compile_features(${APPLICATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${APPLICATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${fastgltf_SOURCE_DIR}/include"
         "${meshoptimizer_SOURCE_DIR}/src"
         "${Stb_SOURCE_DIR}"
         "${SDL2_INCLUDE_DIR}"
         "${SoLoud_SOURCE_DIR}/include"
         "${imgui_SOURCE_DIR}"
         "${imgui_SOURCE_DIR}/backends"
         "${ImGuiFileDialog_SOURCE_DIR}"
         "${ImGuizmo_SOURCE_DIR}"
         "${JoltPhysics_SOURCE_DIR}"
)

target_link_libraries(${APPLICATION_MODULE} PUBLIC ${FOUNDATION_MODULE})
