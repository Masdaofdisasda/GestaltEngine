CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.1
  OPTIONS "GLM_BUILD_TESTS OFF"
)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

set_property(TARGET glm PROPERTY FOLDER "External/")
