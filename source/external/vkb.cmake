CPMAddPackage(
  NAME vk-bootstrap
  GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
  GIT_TAG v1.3.280
  
  OPTIONS
		"DVK_BOOTSTRAP_TEST OFF"
        "DVK_BOOTSTRAP_INSTALL ON"
		"DVK_BOOTSTRAP_VULKAN_HEADER_DIR ${Vulkan_INCLUDE_DIRS}"
)
set_target_properties(vk-bootstrap PROPERTIES VS_GLOBAL_VcpkgEnabled false)

target_include_directories(vk-bootstrap PRIVATE 
    "${Vulkan_INCLUDE_DIRS}"
)


set_property(TARGET vk-bootstrap PROPERTY FOLDER "External/")