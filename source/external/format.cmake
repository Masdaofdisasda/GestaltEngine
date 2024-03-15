CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.0
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS 
      "FORMAT_SKIP_CMAKE YES"
      "FORMAT_SKIP_CLANG NO"
      "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
)


set_property(TARGET check-clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET check-cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET check-format PROPERTY FOLDER "Formatting")
set_property(TARGET clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-clang-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-cmake-format PROPERTY FOLDER "Formatting")
set_property(TARGET fix-format PROPERTY FOLDER "Formatting")
set_property(TARGET format PROPERTY FOLDER "Formatting")