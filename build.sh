#!/bin/bash

# Create and enter build directory
mkdir -p build
cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..
if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    exit 1
fi

# Build the project
cmake --build .
if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

# Run the demos
echo
echo "Running demo..."
./bin/xdemo
echo
echo "Running demov2..."
./bin/xdemov2

cd ..