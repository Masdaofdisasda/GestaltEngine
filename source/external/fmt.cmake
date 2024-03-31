CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
  OPTIONS "FMT_TEST OFF" "FMT_DOC OFF"
)
target_compile_definitions(fmt INTERFACE FMT_SHARED=1)

set_property(TARGET fmt PROPERTY FOLDER "External/")
