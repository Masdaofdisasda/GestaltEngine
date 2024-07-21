CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG  1.3.270
)
set_target_properties(volk PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET volk PROPERTY FOLDER "External/")
