# Create a library for the module
add_library(graphics
	test.hpp
	test.cpp)
# Specify C++20 for modules
target_compile_features(graphics PRIVATE cxx_std_20)

# Include source files or modules
target_sources(graphics PRIVATE
	test.hpp
	test.cpp)
