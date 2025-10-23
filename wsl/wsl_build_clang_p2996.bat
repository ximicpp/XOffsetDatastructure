@echo off
chcp 65001 >nul
REM ============================================================================
REM Build Clang P2996 in WSL2
REM ============================================================================

echo ================================================
echo   Build Clang P2996 (C++26 Reflection)
echo ================================================
echo.

echo This script will:
echo   1. Clone Bloomberg Clang P2996 source code (~2-3 GB)
echo   2. Configure CMake
echo   3. Compile Clang (1-3 hours)
echo   4. Install to ~/clang-p2996-install
echo.
echo Requirements:
echo   - Disk space: 60 GB+
echo   - Time: 1.5-3.5 hours
echo   - RAM: 8 GB+ (16 GB recommended)
echo.

set /p CONTINUE="Start building? [y/n]: "
if /i not "%CONTINUE%"=="y" (
    echo Cancelled
    exit /b 0
)

echo.
echo ================================================
echo   Starting Build (This will take a long time)
echo ================================================
echo.

REM Execute the existing build script
wsl bash /mnt/g/workspace/XOffsetDatastructure/scripts/build_clang_p2996_wsl.sh

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ================================================
    echo   Build Successful!
    echo ================================================
    echo.
    echo Next step:
    echo   Run: wsl_build_project.bat
    echo.
) else (
    echo.
    echo ================================================
    echo   Build Failed
    echo ================================================
    echo.
    echo Please check error messages and retry
    echo.
)

pause