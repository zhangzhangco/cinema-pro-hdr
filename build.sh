#!/bin/bash

# Cinema Pro HDR Build Script

set -e

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring build..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

# Build the project
echo "Building project..."
make -j$(nproc)

# Run tests
echo "Running tests..."
ctest --output-on-failure

echo "Build completed successfully!"