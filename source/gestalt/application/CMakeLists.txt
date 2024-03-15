# Create a library for the module
add_library(${APPLICATION_MODULE}
	test.hpp
	test.cpp)
# Specify C++20 for modules
target_compile_features(${APPLICATION_MODULE} PRIVATE cxx_std_20)

# Include source files or modules
target_sources(${APPLICATION_MODULE} PRIVATE
	test.hpp
	test.cpp)
