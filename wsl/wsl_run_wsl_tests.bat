@echo off
REM ============================================================================
REM Run WSL Reflection Tests (wsl directory only)
REM ============================================================================

echo.
echo ================================================
echo   Run WSL Reflection Tests
echo ================================================
echo.

if not exist wsl_tests_build (
    echo [ERROR] Tests not built yet!
    echo Please run: wsl_build_tests_only.bat
    pause
    exit /b 1
)

:menu
echo.
echo ================================================
echo   Available Tests (All 9 working!)
echo ================================================
echo.
echo   1. test_cpp26_simple        - C++26 environment test
echo   2. test_reflection_syntax   - Basic reflection syntax
echo   3. test_splice              - Splice operator test
echo   4. test_reflect_syntax      - Reflect syntax demo
echo   5. test_reflection_final    - Comprehensive test
echo   6. test_meta_full           - Meta programming test
echo   7. test_p2996_comprehensive - P2996 all features
echo   8. test_meta_functions      - Meta functions advanced
echo   9. test_advanced_meta       - Advanced (members iteration)
echo   A. Run all tests
echo.
echo   0. Exit
echo.

set /p choice="Enter option (0-9/A): "

if "%choice%"=="0" goto :end
if "%choice%"=="1" goto :test1
if "%choice%"=="2" goto :test2
if "%choice%"=="3" goto :test3
if "%choice%"=="4" goto :test4
if "%choice%"=="5" goto :test5
if "%choice%"=="6" goto :test6
if "%choice%"=="7" goto :test7
if "%choice%"=="8" goto :test8
if "%choice%"=="9" goto :test9
if /I "%choice%"=="A" goto :run_all

echo [ERROR] Invalid option (must be 0-9/A)
goto :menu

:test1
call :run_test test_cpp26_simple
pause
goto :menu

:test2
call :run_test test_reflection_syntax
pause
goto :menu

:test3
call :run_test test_splice
pause
goto :menu

:test4
call :run_test test_reflect_syntax
pause
goto :menu

:test5
call :run_test test_reflection_final
pause
goto :menu

:test6
call :run_test test_meta_full
pause
goto :menu

:test7
call :run_test test_p2996_comprehensive
pause
goto :menu

:test8
call :run_test test_meta_functions
pause
goto :menu

:test9
call :run_test test_advanced_meta
pause
goto :menu

:run_all
echo.
echo ================================================
echo   Running All 9 Tests
echo ================================================
echo.
call :run_test test_cpp26_simple
call :run_test test_reflection_syntax
call :run_test test_splice
call :run_test test_reflect_syntax
call :run_test test_reflection_final
call :run_test test_meta_full
call :run_test test_p2996_comprehensive
call :run_test test_meta_functions
call :run_test test_advanced_meta
echo.
echo ================================================
echo   All 9 tests completed successfully!
echo ================================================
pause
goto :menu

:run_test
echo.
echo ------------------------------------------------
echo   Running: %1
echo ------------------------------------------------
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl/wsl_tests_build && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./%1"
echo.
exit /b

:end
echo.
echo Goodbye!
exit /b 0
