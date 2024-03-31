CPMAddPackage(
  NAME VMA
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.0.1
)
set_target_properties(VulkanMemoryAllocator PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET VulkanMemoryAllocator PROPERTY FOLDER "External/")
