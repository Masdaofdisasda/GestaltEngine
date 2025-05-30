message(STATUS "Configuring Foundation module...")

# automate file collection and source grouping
function(add_sources_from_folder folder_name)
  file(GLOB_RECURSE SOURCES_IN_FOLDER
       "${CMAKE_CURRENT_SOURCE_DIR}/${folder_name}/*.hpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/${folder_name}/*.cpp")

  list(APPEND ALL_SOURCES ${SOURCES_IN_FOLDER})

  string(REPLACE "/" "\\" group_name "${folder_name}")
  source_group("${group_name}" FILES ${SOURCES_IN_FOLDER})
endfunction()

# NOTE: source code will be added from these folders:
set(FOLDERS
    "Components"
    "Mesh"
    "Light"
    "Material"
    "Resources"
    "Buffer"
    "Animation"
    "Descriptor"
    "Interface")

foreach(folder ${FOLDERS})
  add_sources_from_folder(${folder})
endforeach()

# Add main source files from root directory
file(GLOB_RECURSE ROOT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(APPEND ALL_SOURCES ${ROOT_SOURCES})
source_group("Source Files" FILES ${ROOT_SOURCES})

add_library(Foundation ${ALL_SOURCES})
enable_engine_cxx_standard(Foundation)
# enable_engine_warnings(Foundation)

target_include_directories(
  Foundation
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${VulkanUtilityLibraries_SOURCE_DIR}/include" # only available on linux,
                                                   # needed for
                                                   # vk_enum_string_helper.h
)

target_link_libraries(Foundation PUBLIC glm::glm Vulkan::Headers Vulkan::UtilityHeaders volk::volk 
                                        GPUOpen::VulkanMemoryAllocator)
target_link_libraries(Foundation PRIVATE fmt::fmt Vulkan::Loader
                                         nlohmann_json::nlohmann_json)

set_folder(Foundation "Engine/")
