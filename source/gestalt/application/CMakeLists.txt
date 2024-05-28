message(STATUS "Configuring ${APPLICATION_MODULE} module...")

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(NOT SOURCE_FILES)
  message(FATAL_ERROR "No source files found for the ${APPLICATION_MODULE} module")
endif()

add_library(${APPLICATION_MODULE} ${HEADER_FILES} ${SOURCE_FILES})

target_compile_features(${APPLICATION_MODULE} PRIVATE cxx_std_20)

target_include_directories(
  ${APPLICATION_MODULE}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
         "${fastgltf_SOURCE_DIR}/include"
         "${meshoptimizer_SOURCE_DIR}/src"
         "${Stb_SOURCE_DIR}"
         "${SDL2_INCLUDE_DIR}"
         "${imgui_SOURCE_DIR}"
         "${imgui_SOURCE_DIR}/backends"
         "${ImGuiFileDialog_SOURCE_DIR}"
         "${ImGuizmo_SOURCE_DIR}"
)

target_link_libraries(${APPLICATION_MODULE} PUBLIC ${FOUNDATION_MODULE})
