# Create a library for the module
add_library(application
	test.hpp
	test.cpp)
# Specify C++20 for modules
target_compile_features(application PRIVATE cxx_std_20)

# Include source files or modules
target_sources(application PRIVATE
	test.hpp
	test.cpp)
