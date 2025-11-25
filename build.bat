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

:: Define test list (name^|display name)
set TEST_COUNT=0
set TESTS[0]=test_basic_types^|Basic Types Test
set TESTS[1]=test_vector^|Vector Test
set TESTS[2]=test_map_set^|Map/Set Test
set TESTS[3]=test_nested^|Nested Structures Test
set TESTS[4]=test_compaction^|Memory Compaction Test
set TESTS[5]=test_mmap_loading^|Mmap Loading Test
set TEST_COUNT=6

:: Run main tests
for /L %%i in (0,1,5) do (
    call :run_test %%i 6
)

:: Run bonus test
call :run_bonus_test test_msvc_compat "MSVC Compatibility Test"

goto :after_tests

:run_test
set INDEX=%1
set TOTAL=%2
setlocal enabledelayedexpansion
for /f "tokens=1,2 delims=^|" %%a in ("!TESTS[%INDEX%]!") do (
    set TEST_NAME=%%a
    set DISPLAY_NAME=%%b
    set /a TEST_NUM=%INDEX%+1
    if exist "bin\Release\!TEST_NAME!.exe" (
        echo.
        echo [!TEST_NUM!/%TOTAL%] Running !DISPLAY_NAME!...
        bin\Release\!TEST_NAME!.exe
        if errorlevel 1 set TEST_FAILED=1
    ) else (
        echo [!TEST_NUM!/%TOTAL%] !TEST_NAME!.exe not found (skipped)
    )
)
endlocal & set TEST_FAILED=%TEST_FAILED%
goto :eof

:run_bonus_test
set TEST_NAME=%~1
set DISPLAY_NAME=%~2
if exist "bin\Release\%TEST_NAME%.exe" (
    echo.
    echo [Bonus] Running %DISPLAY_NAME%...
    bin\Release\%TEST_NAME%.exe
    if errorlevel 1 set TEST_FAILED=1
) else (
    echo [Bonus] %TEST_NAME%.exe not found (skipped)
)
goto :eof

:after_tests

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
