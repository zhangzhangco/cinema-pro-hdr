cmake_minimum_required(VERSION 3.16)

# Find or download a simple testing framework
# For simplicity, we'll create a minimal testing framework
# In production, you might want to use Google Test, Catch2, etc.

# Test sources
set(TEST_SOURCES
    test_main.cpp
    test_cph_params.cpp
    test_image.cpp
    test_statistics.cpp
    test_error_report.cpp
    test_color_space.cpp
    test_pq_functions.cpp
    test_processor.cpp
    test_numerical_precision.cpp
    test_parameter_validation.cpp
)

# Create test executable
add_executable(cinema_pro_hdr_tests ${TEST_SOURCES})

# Link with core library
target_link_libraries(cinema_pro_hdr_tests cinema_pro_hdr_core)

# Include directories
target_include_directories(cinema_pro_hdr_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add test target
add_test(NAME cinema_pro_hdr_unit_tests COMMAND cinema_pro_hdr_tests)

# Compiler flags for tests
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(cinema_pro_hdr_tests PRIVATE -Wall -Wextra -g)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(cinema_pro_hdr_tests PRIVATE /W4 /Zi)
endif()