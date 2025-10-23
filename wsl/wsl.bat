@echo off
REM ============================================================================
REM XOffsetDatastructure2 - WSL Build and Test Entry Script
REM ============================================================================

echo.
echo ================================================
echo   XOffsetDatastructure2 - WSL Tools
echo ================================================
echo.
echo This project uses WSL2 for C++26 reflection development
echo.
echo Available operations:
echo   1. Initialize environment (first time only)
echo   2. Build project
echo   3. Run tests
echo   4. View documentation
echo   0. Exit
echo.

set /p choice="Please select an option (0-4): "

if "%choice%"=="0" goto :end
if "%choice%"=="1" goto :init
if "%choice%"=="2" goto :build
if "%choice%"=="3" goto :test
if "%choice%"=="4" goto :docs

echo [ERROR] Invalid option
pause
exit /b 1

:init
echo.
echo ================================================
echo   Initialize WSL Environment
echo ================================================
echo.
call wsl_setup_tools.bat
call wsl_build_clang_p2996.bat
pause
exit /b

:build
echo.
echo ================================================
echo   Build Project
echo ================================================
echo.
call wsl_rebuild_with_reflection.bat
pause
exit /b

:test
echo.
echo ================================================
echo   Run Tests
echo ================================================
echo.
call wsl_run_tests.bat
exit /b

:docs
echo.
echo ================================================
echo   Documentation
echo ================================================
echo.
echo - Quick Start: WSL2_QUICK_START.md
echo - WSL Tools: README.md
echo - Project Home: ..\README.md
echo.
pause
exit /b

:end
echo.
echo Goodbye!
exit /b 0
