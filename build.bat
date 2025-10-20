@echo off
setlocal enabledelayedexpansion

:: Create and enter build directory
if not exist "build" mkdir build
cd build

:: Configure with CMake
echo Configuring CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

:: Build the project
echo Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

:: Run tests first
echo.
echo ======================================================================
echo Running Tests
echo ======================================================================

set TEST_FAILED=0

if exist "bin\Release\test_basic_types.exe" (
    echo.
    echo [1/5] Running Basic Types Test...
    bin\Release\test_basic_types.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [1/5] test_basic_types.exe not found (skipped)
)

if exist "bin\Release\test_vector.exe" (
    echo.
    echo [2/5] Running Vector Test...
    bin\Release\test_vector.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [2/5] test_vector.exe not found (skipped)
)

if exist "bin\Release\test_map_set.exe" (
    echo.
    echo [3/5] Running Map/Set Test...
    bin\Release\test_map_set.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [3/5] test_map_set.exe not found (skipped)
)

if exist "bin\Release\test_nested.exe" (
    echo.
    echo [4/5] Running Nested Structures Test...
    bin\Release\test_nested.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [4/5] test_nested.exe not found (skipped)
)

if exist "bin\Release\test_compaction.exe" (
    echo.
    echo [5/5] Running Memory Compaction Test...
    bin\Release\test_compaction.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [5/5] test_compaction.exe not found (skipped)
)

:: Run MSVC compatibility test
if exist "bin\Release\test_msvc_compat.exe" (
    echo.
    echo [Bonus] Running MSVC Compatibility Test...
    bin\Release\test_msvc_compat.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [Bonus] test_msvc_compat.exe not found (skipped)
)

:: Run the demo after tests
echo.
echo ======================================================================
echo Running XOffsetDatastructure Demo
echo ======================================================================
if exist "bin\Release\demo.exe" (
    bin\Release\demo.exe
) else (
    echo demo.exe not found!
    set TEST_FAILED=1
)

:: Return to original directory
cd ..

:: Final summary
echo.
echo ======================================================================
if %TEST_FAILED%==0 (
    echo Build, demo, and tests completed successfully!
    echo Result: ALL TESTS PASSED
) else (
    echo Build and demo completed, but some tests FAILED
    echo Result: TESTS FAILED
)
echo ======================================================================
echo.

exit /b %TEST_FAILED%
