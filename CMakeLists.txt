cmake_minimum_required(VERSION 3.16)
project(CinemaProHDR VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
endif()

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_OFX "Build OFX plugin" ON)
option(BUILD_TOOLS "Build command line tools" ON)
option(USE_CUDA "Enable CUDA support" OFF)
option(USE_METAL "Enable Metal support" OFF)

# Find packages
find_package(Threads REQUIRED)

# CUDA support
if(USE_CUDA)
    find_package(CUDA QUIET)
    if(CUDA_FOUND)
        enable_language(CUDA)
        add_definitions(-DUSE_CUDA)
    endif()
endif()

# Metal support for macOS
if(USE_METAL AND PLATFORM_MACOS)
    add_definitions(-DUSE_METAL)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library sources
set(CORE_SOURCES
    src/core/cph_params.cpp
    src/core/image.cpp
    src/core/statistics.cpp
    src/core/error_report.cpp
    src/core/color_space.cpp
    src/core/pq_functions.cpp
    src/core/cph_processor.cpp
)

# Core library
add_library(cinema_pro_hdr_core SHARED ${CORE_SOURCES})
target_include_directories(cinema_pro_hdr_core PUBLIC include)
target_link_libraries(cinema_pro_hdr_core Threads::Threads)

# Platform-specific optimizations
if(PLATFORM_WINDOWS)
    target_compile_definitions(cinema_pro_hdr_core PRIVATE PLATFORM_WINDOWS)
elseif(PLATFORM_MACOS)
    target_compile_definitions(cinema_pro_hdr_core PRIVATE PLATFORM_MACOS)
elseif(PLATFORM_LINUX)
    target_compile_definitions(cinema_pro_hdr_core PRIVATE PLATFORM_LINUX)
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(cinema_pro_hdr_core PRIVATE -Wall -Wextra -O3)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(cinema_pro_hdr_core PRIVATE -g -O0)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(cinema_pro_hdr_core PRIVATE /W4 /O2)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(cinema_pro_hdr_core PRIVATE /Od /Zi)
    endif()
endif()

# Unit tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# OFX plugin
if(BUILD_OFX)
    add_subdirectory(src/ofx)
endif()

# Command line tools
if(BUILD_TOOLS)
    add_subdirectory(src/tools)
endif()

# Install targets
install(TARGETS cinema_pro_hdr_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)