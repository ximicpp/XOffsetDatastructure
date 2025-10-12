#!/bin/bash

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

# Run tests first
echo
echo "======================================================================"
echo "Running Tests"
echo "======================================================================"

TEST_FAILED=0

if [ -f "bin/test_basic_types" ]; then
    echo
    echo "[1/5] Running Basic Types Test..."
    ./bin/test_basic_types || TEST_FAILED=1
else
    echo "[1/5] test_basic_types not found (skipped)"
fi

if [ -f "bin/test_vector" ]; then
    echo
    echo "[2/5] Running Vector Test..."
    ./bin/test_vector || TEST_FAILED=1
else
    echo "[2/5] test_vector not found (skipped)"
fi

if [ -f "bin/test_map_set" ]; then
    echo
    echo "[3/5] Running Map/Set Test..."
    ./bin/test_map_set || TEST_FAILED=1
else
    echo "[3/5] test_map_set not found (skipped)"
fi

if [ -f "bin/test_nested" ]; then
    echo
    echo "[4/5] Running Nested Structures Test..."
    ./bin/test_nested || TEST_FAILED=1
else
    echo "[4/5] test_nested not found (skipped)"
fi

if [ -f "bin/test_compaction" ]; then
    echo
    echo "[5/5] Running Memory Compaction Test..."
    ./bin/test_compaction || TEST_FAILED=1
else
    echo "[5/5] test_compaction not found (skipped)"
fi

# Run the demo after tests
echo
echo "======================================================================"
echo "Running XOffsetDatastructure Demo v2"
echo "======================================================================"
./bin/xoffsetdatastructure2_demo --visualize --compact

# Return to original directory
cd ..

# Final summary
echo
echo "======================================================================"
if [ $TEST_FAILED -eq 0 ]; then
    echo "Build, demo, and tests completed successfully!"
    echo "Result: ALL TESTS PASSED"
else
    echo "Build and demo completed, but some tests FAILED"
    echo "Result: TESTS FAILED"
fi
echo "======================================================================"
echo

exit $TEST_FAILED
