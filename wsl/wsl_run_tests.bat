@echo off
REM ============================================================================
REM Run XOffsetDatastructure2 Tests and Examples
REM ============================================================================

echo.
echo ================================================
echo   XOffsetDatastructure2 Test Suite
echo ================================================
echo.

set BUILD_DIR=..\build_cpp26\bin

if not exist %BUILD_DIR% (
    echo [ERROR] Build directory not found: %BUILD_DIR%
    echo Please run: wsl_rebuild_with_reflection.bat
    pause
    exit /b 1
)

echo [INFO] Test directory: %BUILD_DIR%
echo.

:menu
echo ================================================
echo   Select test or example to run:
echo ================================================
echo.
echo   1. Run all tests (recommended)
echo   2. Hello World example
echo   3. Basic types test
echo   4. Vector container test
echo   5. Nested structures test
echo   6. Map/Set container test
echo   7. Modify operations test
echo   8. Compaction test
echo   9. Full demo
echo   0. Exit
echo.
set /p choice="Please enter option (0-9): "

if "%choice%"=="0" goto :end
if "%choice%"=="1" goto :run_all
if "%choice%"=="2" goto :run_helloworld
if "%choice%"=="3" goto :run_basic_types
if "%choice%"=="4" goto :run_vector
if "%choice%"=="5" goto :run_nested
if "%choice%"=="6" goto :run_map_set
if "%choice%"=="7" goto :run_modify
if "%choice%"=="8" goto :run_compaction
if "%choice%"=="9" goto :run_demo

echo [ERROR] Invalid option
echo.
goto :menu

:run_all
echo.
echo ================================================
echo   Run All Tests
echo ================================================
echo.
call :run_test helloworld
call :run_test test_basic_types
call :run_test test_vector
call :run_test test_nested
call :run_test test_map_set
call :run_test test_modify
call :run_test test_compaction
call :run_test xoffsetdatastructure2_demo
echo.
echo ================================================
echo   All Tests Complete!
echo ================================================
pause
goto :menu

:run_helloworld
call :run_single_test helloworld
goto :menu

:run_basic_types
call :run_single_test test_basic_types
goto :menu

:run_vector
call :run_single_test test_vector
goto :menu

:run_nested
call :run_single_test test_nested
goto :menu

:run_map_set
call :run_single_test test_map_set
goto :menu

:run_modify
call :run_single_test test_modify
goto :menu

:run_compaction
call :run_single_test test_compaction
goto :menu

:run_demo
call :run_single_test xoffsetdatastructure2_demo
goto :menu

:run_single_test
echo.
echo ------------------------------------------------
echo   Running: %1
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/%1"
echo.
echo [DONE] %1
pause
exit /b

:run_test
echo.
echo ------------------------------------------------
echo   Running: %1
echo ------------------------------------------------
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/%1"
echo.
exit /b

:end
echo.
echo Goodbye!
exit /b 0
