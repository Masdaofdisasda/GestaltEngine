# Create a library for the module
add_library(foundation
	test.hpp
	test.cpp)
# Specify C++20 for modules
target_compile_features(foundation PRIVATE cxx_std_20)

# Include source files or modules
target_sources(foundation PRIVATE
	test.hpp
	test.cpp)
