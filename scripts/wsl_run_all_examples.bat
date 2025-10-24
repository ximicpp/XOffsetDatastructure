@echo off
REM ============================================================================
REM Run All Examples - Quick Demo
REM ============================================================================

echo.
echo ================================================
echo   Running All Examples
echo ================================================
echo.

if not exist ..\build_cpp26\bin (
    echo [ERROR] Build directory not found!
    echo Please run: wsl_rebuild_with_reflection.bat
    pause
    exit /b 1
)

echo [1/8] Hello World...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/helloworld"
echo.

echo [2/8] Basic Types Test...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_basic_types"
echo.

echo [3/8] Vector Container Test...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_vector"
echo.

echo [4/8] Nested Structures Test...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_nested"
echo.

echo [5/8] Map/Set Container Test...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_map_set"
echo.

echo [6/8] Modify Operations Test...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_modify"
echo.

echo [7/8] Compaction Test...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_compaction"
echo.

echo [8/8] Full Demo...
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/xoffsetdatastructure2_demo"
echo.

echo ================================================
echo   All Examples Complete!
echo ================================================
echo.
pause
