CPMAddPackage(
  NAME VMA
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.0.1
)

set_property(TARGET VulkanMemoryAllocator PROPERTY FOLDER "External/")