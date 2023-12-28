add_subdirectory(external)
add_subdirectory(gestalt/application)
add_subdirectory(gestalt/foundation)
add_subdirectory(gestalt/graphics)

add_executable(GestaltApp gestalt/main.cpp)

# Specify C++20, this is required for modules
target_compile_features(GestaltApp PRIVATE cxx_std_20)

# Link with other modules/libraries
target_link_libraries(GestaltApp PRIVATE SDL2::SDL2 Vulkan::Vulkan application foundation graphics)
