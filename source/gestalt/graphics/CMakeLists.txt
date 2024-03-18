message(STATUS "Configuring graphics module...")

file(GLOB_RECURSE HEADER_FILES 
     "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE SOURCE_FILES 
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${GRAPHICS_MODULE}
    ${HEADER_FILES}
    ${SOURCE_FILES})

target_compile_features(${GRAPHICS_MODULE} PRIVATE cxx_std_20)

target_include_directories(${GRAPHICS_MODULE} PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${Vulkan_INCLUDE_DIRS}"
	"${Stb_SOURCE_DIR}"
	"${glm_SOURCE_DIR}"
    "${fastgltf_SOURCE_DIR}/include"
    "${fmt_SOURCE_DIR}/include"
    "${vk-bootstrap_SOURCE_DIR}/src"
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${ImGuiFileDialog_SOURCE_DIR}"
    "${ImGuizmo_SOURCE_DIR}"
    "${SDL2_INCLUDE_DIR}"
)

