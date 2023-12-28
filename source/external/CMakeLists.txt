# Create a library for the module
add_library(external
	test.hpp
	test.cpp)
# Specify C++20 for modules
target_compile_features(external PRIVATE cxx_std_20)

# Include source files or modules
target_sources(external PRIVATE
	test.hpp
	test.cpp)
