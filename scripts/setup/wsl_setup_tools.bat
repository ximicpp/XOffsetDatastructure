@echo off
chcp 65001 >nul
REM ============================================================================
REM WSL2 Tools Installation Script
REM ============================================================================

echo ================================================
echo   Install WSL2 Build Tools
echo ================================================
echo.

echo This script will install in WSL2:
echo   - build-essential (GCC, Make, etc.)
echo   - cmake
echo   - ninja-build
echo   - git
echo   - python3
echo.

set /p CONTINUE="Continue with installation? [y/n]: "
if /i not "%CONTINUE%"=="y" (
    echo Cancelled
    exit /b 0
)

echo.
echo [1/3] Updating package list...
wsl sudo apt update

echo.
echo [2/3] Installing build tools...
wsl sudo apt install -y build-essential cmake ninja-build git python3

echo.
echo [3/3] Verifying installation...
wsl bash -c "echo 'GCC:' && gcc --version | head -1"
wsl bash -c "echo 'CMake:' && cmake --version | head -1"
wsl bash -c "echo 'Ninja:' && ninja --version"
wsl bash -c "echo 'Git:' && git --version"
wsl bash -c "echo 'Python:' && python3 --version"

echo.
echo ================================================
echo   Tools Installation Complete!
echo ================================================
echo.
echo Next step:
echo   Run: wsl_build_clang_p2996.bat
echo.

pause
