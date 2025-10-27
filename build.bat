@echo off
setlocal enabledelayedexpansion

echo.
echo ======================================================================
echo   XOffsetDatastructure2 Build Script (with Reflection Support)
echo ======================================================================
echo.

:: Parse command line arguments
set USE_WSL=1
set ENABLE_REFLECTION=1
set BUILD_TYPE=Release
set SHOW_HELP=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--no-wsl" (
    set USE_WSL=0
    shift
    goto parse_args
)
if /i "%~1"=="--no-reflection" (
    set ENABLE_REFLECTION=0
    shift
    goto parse_args
)
if /i "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    set SHOW_HELP=1
    shift
    goto parse_args
)
if /i "%~1"=="-h" (
    set SHOW_HELP=1
    shift
    goto parse_args
)
shift
goto parse_args

:args_done

:: Show help if requested
if %SHOW_HELP%==1 (
    echo Usage: build.bat [OPTIONS]
    echo.
    echo Options:
    echo   --no-wsl          Use Visual Studio compiler instead of WSL Clang
    echo   --no-reflection   Disable C++26 reflection tests
    echo   --debug           Build in Debug mode instead of Release
    echo   --help, -h        Show this help message
    echo.
    echo Default: Use WSL Clang with reflection enabled in Release mode
    echo.
    echo Examples:
    echo   build.bat                    - Build with WSL Clang and reflection
    echo   build.bat --no-wsl           - Build with Visual Studio
    echo   build.bat --no-reflection    - Build without reflection tests
    echo   build.bat --debug            - Build in Debug mode
    echo.
    exit /b 0
)

:: Display configuration
echo Configuration:
if %USE_WSL%==1 (
    echo   Compiler: WSL Clang with P2996 support
) else (
    echo   Compiler: Visual Studio 2022
)
if %ENABLE_REFLECTION%==1 (
    echo   Reflection: ENABLED
) else (
    echo   Reflection: DISABLED
)
echo   Build Type: %BUILD_TYPE%
echo.

:: Create and enter build directory
if not exist "build" mkdir build
cd build

:: Configure with CMake based on compiler choice
if %USE_WSL%==1 (
    echo Configuring CMake with WSL Clang...
    echo.
    
    :: WSL path to workspace
    set WSL_WORKSPACE=/mnt/g/workspace/XOffsetDatastructure
    
    :: Configure CMake to use WSL Clang
    if %ENABLE_REFLECTION%==1 (
        %SystemRoot%\System32\wsl.exe bash -c "cd %WSL_WORKSPACE% && cmake -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_CXX_COMPILER=~/clang-p2996-install/bin/clang++ -DCMAKE_C_COMPILER=~/clang-p2996-install/bin/clang -DENABLE_REFLECTION_TESTS=ON -DCMAKE_CXX_FLAGS='-stdlib=libc++' -DCMAKE_EXE_LINKER_FLAGS='-L$HOME/clang-p2996-install/lib -Wl,-rpath,$HOME/clang-p2996-install/lib'"
    ) else (
        %SystemRoot%\System32\wsl.exe bash -c "cd %WSL_WORKSPACE% && cmake -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_CXX_COMPILER=~/clang-p2996-install/bin/clang++ -DCMAKE_C_COMPILER=~/clang-p2996-install/bin/clang -DENABLE_REFLECTION_TESTS=OFF -DCMAKE_CXX_FLAGS='-stdlib=libc++' -DCMAKE_EXE_LINKER_FLAGS='-L$HOME/clang-p2996-install/lib -Wl,-rpath,$HOME/clang-p2996-install/lib'"
    )
    
    if errorlevel 1 (
        echo CMake configuration failed
        cd ..
        exit /b 1
    )
    
    echo.
    echo Building project with WSL Clang...
    echo.
    %SystemRoot%\System32\wsl.exe bash -c "cd %WSL_WORKSPACE% && cmake --build build --config %BUILD_TYPE% -j$(nproc)"
    
    if errorlevel 1 (
        echo Build failed
        cd ..
        exit /b 1
    )
) else (
    echo Configuring CMake with Visual Studio...
    echo.
    
    if %ENABLE_REFLECTION%==1 (
        echo WARNING: Visual Studio does not support C++26 reflection
        echo Reflection tests will be disabled
        cmake -G "Visual Studio 17 2022" -A x64 -DENABLE_REFLECTION_TESTS=OFF ..
    ) else (
        cmake -G "Visual Studio 17 2022" -A x64 -DENABLE_REFLECTION_TESTS=OFF ..
    )
    
    if errorlevel 1 (
        echo CMake configuration failed
        cd ..
        exit /b 1
    )
    
    echo.
    echo Building project with Visual Studio...
    echo.
    cmake --build . --config %BUILD_TYPE%
    
    if errorlevel 1 (
        echo Build failed
        cd ..
        exit /b 1
    )
)

:: Run tests
echo.
echo ======================================================================
echo Running Tests
echo ======================================================================
echo.

set TEST_FAILED=0
set TEST_COUNT=0
set PASSED_COUNT=0

:: Determine test executable path based on compiler and build type
if %USE_WSL%==1 (
    set TEST_PATH=bin/%BUILD_TYPE%
) else (
    set TEST_PATH=bin\%BUILD_TYPE%
)

:: Basic tests (6 tests)
echo === Basic Tests ===
echo.

set /a TEST_COUNT+=1
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/test_basic_types" (
        echo [%TEST_COUNT%/14] Running Basic Types Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_basic_types"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_basic_types not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\test_basic_types.exe" (
        echo [%TEST_COUNT%/14] Running Basic Types Test...
        %TEST_PATH%\test_basic_types.exe
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_basic_types.exe not found (skipped)
    )
)

set /a TEST_COUNT+=1
echo.
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/test_vector" (
        echo [%TEST_COUNT%/14] Running Vector Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_vector"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_vector not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\test_vector.exe" (
        echo [%TEST_COUNT%/14] Running Vector Test...
        %TEST_PATH%\test_vector.exe
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_vector.exe not found (skipped)
    )
)

set /a TEST_COUNT+=1
echo.
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/test_map_set" (
        echo [%TEST_COUNT%/14] Running Map/Set Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_map_set"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_map_set not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\test_map_set.exe" (
        echo [%TEST_COUNT%/14] Running Map/Set Test...
        %TEST_PATH%\test_map_set.exe
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_map_set.exe not found (skipped)
    )
)

set /a TEST_COUNT+=1
echo.
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/test_nested" (
        echo [%TEST_COUNT%/14] Running Nested Structures Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_nested"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_nested not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\test_nested.exe" (
        echo [%TEST_COUNT%/14] Running Nested Structures Test...
        %TEST_PATH%\test_nested.exe
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_nested.exe not found (skipped)
    )
)

set /a TEST_COUNT+=1
echo.
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/test_compaction" (
        echo [%TEST_COUNT%/14] Running Memory Compaction Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_compaction"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_compaction not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\test_compaction.exe" (
        echo [%TEST_COUNT%/14] Running Memory Compaction Test...
        %TEST_PATH%\test_compaction.exe
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_compaction.exe not found (skipped)
    )
)

set /a TEST_COUNT+=1
echo.
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/test_modify" (
        echo [%TEST_COUNT%/14] Running Data Modification Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_modify"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_modify not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\test_modify.exe" (
        echo [%TEST_COUNT%/14] Running Data Modification Test...
        %TEST_PATH%\test_modify.exe
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_modify.exe not found (skipped)
    )
)

:: Reflection tests (8 tests) - only if enabled and WSL
if %ENABLE_REFLECTION%==1 if %USE_WSL%==1 (
    echo.
    echo === Reflection Tests ===
    echo.
    
    set /a TEST_COUNT+=1
    if exist "bin/%BUILD_TYPE%/test_reflection_operators" (
        echo [%TEST_COUNT%/14] Running Reflection Operators Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_reflection_operators"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_reflection_operators not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_member_iteration" (
        echo [%TEST_COUNT%/14] Running Member Iteration Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_member_iteration"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_member_iteration not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_reflection_type_signature" (
        echo [%TEST_COUNT%/14] Running Reflection Type Signature Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_reflection_type_signature"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_reflection_type_signature not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_splice_operations" (
        echo [%TEST_COUNT%/14] Running Splice Operations Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_splice_operations"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_splice_operations not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_type_introspection" (
        echo [%TEST_COUNT%/14] Running Type Introspection Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_type_introspection"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_type_introspection not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_reflection_compaction" (
        echo [%TEST_COUNT%/14] Running Reflection Compaction Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_reflection_compaction"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_reflection_compaction not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_reflection_serialization" (
        echo [%TEST_COUNT%/14] Running Reflection Serialization Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_reflection_serialization"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_reflection_serialization not found (skipped)
    )
    
    set /a TEST_COUNT+=1
    echo.
    if exist "bin/%BUILD_TYPE%/test_reflection_comparison" (
        echo [%TEST_COUNT%/14] Running Reflection Comparison Test...
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/test_reflection_comparison"
        if !errorlevel!==0 (set /a PASSED_COUNT+=1) else (set TEST_FAILED=1)
    ) else (
        echo [%TEST_COUNT%/14] test_reflection_comparison not found (skipped)
    )
)

:: Run the demo after tests
echo.
echo ======================================================================
echo Running XOffsetDatastructure Demo v2
echo ======================================================================
echo.
if %USE_WSL%==1 (
    if exist "bin/%BUILD_TYPE%/xoffsetdatastructure2_demo" (
        %SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build && ./bin/%BUILD_TYPE%/xoffsetdatastructure2_demo --visualize --compact"
    ) else (
        echo Demo executable not found (skipped)
    )
) else (
    if exist "%TEST_PATH%\xoffsetdatastructure2_demo.exe" (
        %TEST_PATH%\xoffsetdatastructure2_demo.exe --visualize --compact
    ) else (
        echo Demo executable not found (skipped)
    )
)

:: Return to original directory
cd ..

:: Final summary
echo.
echo ======================================================================
echo   Build Summary
echo ======================================================================
echo.
echo   Tests Run: %TEST_COUNT%
echo   Tests Passed: %PASSED_COUNT%
if %TEST_FAILED%==0 (
    echo   Result: ALL TESTS PASSED
    echo.
    echo   Status: SUCCESS
) else (
    set /a FAILED_COUNT=%TEST_COUNT%-%PASSED_COUNT%
    echo   Tests Failed: !FAILED_COUNT!
    echo   Result: SOME TESTS FAILED
    echo.
    echo   Status: FAILED
)
echo ======================================================================
echo.

if %TEST_FAILED%==0 (
    echo Build, demo, and tests completed successfully!
) else (
    echo Build and demo completed, but some tests FAILED
)
echo.

exit /b %TEST_FAILED%
