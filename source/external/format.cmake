CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.0
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS "FORMAT_SKIP_CMAKE YES" "FORMAT_SKIP_CLANG NO" "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
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


file(GLOB_RECURSE ALL_SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/source/gestalt/*.cpp
    ${CMAKE_SOURCE_DIR}/source/gestalt/*.h
    ${CMAKE_SOURCE_DIR}/source/gestalt/graphics/*.cpp
    ${CMAKE_SOURCE_DIR}/source/gestaltgraphics/*.h
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Custom target to run clang-tidy on all source files
add_custom_target(
    clang-tidy
    COMMAND clang-tidy
    ${ALL_SOURCE_FILES}
    -p ${CMAKE_BINARY_DIR} 
    -- 
    # Add any additional clang-tidy options here, if needed.
)

add_custom_target(
    fix-clang-tidy
    COMMAND clang-tidy
    -p ${CMAKE_BINARY_DIR}
    --fix-errors 
    ${ALL_SOURCE_FILES}
    --
    # After '--', you can add additional arguments to pass to the compiler if needed
)

set_property(TARGET clang-tidy PROPERTY FOLDER "Linting")
set_property(TARGET fix-clang-tidy PROPERTY FOLDER "Linting")
