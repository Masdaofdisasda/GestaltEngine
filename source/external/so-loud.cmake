CPMAddPackage(
  NAME SoLoud
  GITHUB_REPOSITORY jarikomppa/soloud
  GIT_TAG RELEASE_20200207
  SOURCE_SUBDIR contrib
  OPTIONS "WITH_SDL2_STATIC ON" # Use SDL2 for audio backend
)

set_property(TARGET soloud PROPERTY FOLDER "External")