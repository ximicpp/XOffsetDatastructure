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

# Determine test binary directory (macOS/Linux uses Release, Windows uses Release)
if [ -d "bin/Release" ]; then
    TEST_DIR="bin/Release"
elif [ -d "bin" ]; then
    TEST_DIR="bin"
else
    TEST_DIR=""
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_basic_types" ]; then
    echo
    echo "[1/5] Running Basic Types Test..."
    ./$TEST_DIR/test_basic_types || TEST_FAILED=1
else
    echo "[1/5] test_basic_types not found (skipped)"
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_vector" ]; then
    echo
    echo "[2/5] Running Vector Test..."
    ./$TEST_DIR/test_vector || TEST_FAILED=1
else
    echo "[2/5] test_vector not found (skipped)"
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_map_set" ]; then
    echo
    echo "[3/5] Running Map/Set Test..."
    ./$TEST_DIR/test_map_set || TEST_FAILED=1
else
    echo "[3/5] test_map_set not found (skipped)"
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_nested" ]; then
    echo
    echo "[4/5] Running Nested Structures Test..."
    ./$TEST_DIR/test_nested || TEST_FAILED=1
else
    echo "[4/5] test_nested not found (skipped)"
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_compaction" ]; then
    echo
    echo "[5/6] Running Memory Statistics Test..."
    ./$TEST_DIR/test_compaction || TEST_FAILED=1
else
    echo "[5/6] test_compaction not found (skipped)"
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_modify" ]; then
    echo
    echo "[6/7] Running Data Modification Test..."
    ./$TEST_DIR/test_modify || TEST_FAILED=1
else
    echo "[6/7] test_modify not found (skipped)"
fi

if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/test_comprehensive" ]; then
    echo
    echo "[7/7] Running Comprehensive Integration Test..."
    ./$TEST_DIR/test_comprehensive || TEST_FAILED=1
else
    echo "[7/7] test_comprehensive not found (skipped)"
fi

# Run the demo after tests
echo
echo "======================================================================"
echo "Running XOffsetDatastructure Demo"
echo "======================================================================"
if [ -n "$TEST_DIR" ] && [ -f "$TEST_DIR/../demo" ]; then
    $TEST_DIR/../demo
elif [ -f "bin/demo" ]; then
    ./bin/demo
else
    echo "Demo not found (skipped)"
fi

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
