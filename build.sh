#!/bin/bash

# Exit on error
set -e

# Create and enter build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the project
echo "Building project..."
cmake --build .

# Run the demos
echo
echo "Running demo v1..."
./bin/xoffsetdatastructure_demo
echo
echo "Running demo v2..."
./bin/xoffsetdatastructure2_demo

# Return to original directory
cd ..

echo
echo "Build and run completed successfully!"