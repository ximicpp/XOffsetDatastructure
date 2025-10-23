@echo off
chcp 65001 >nul
REM ============================================================================
REM Upgrade CMake in WSL2
REM ============================================================================

echo ================================================
echo   Upgrade CMake to Latest Version
echo ================================================
echo.

echo Current CMake version is too old (3.16.3)
echo Clang P2996 requires CMake 3.20.0 or higher
echo.
echo This script will:
echo   1. Remove old CMake (3.16.3)
echo   2. Download pre-built CMake 3.27.7
echo   3. Install (1-2 minutes)
echo   4. Verify installation
echo.

set /p CONTINUE="Continue with CMake upgrade? [y/n]: "
if /i not "%CONTINUE%"=="y" (
    echo Cancelled
    exit /b 0
)

echo.
echo ================================================
echo   Starting CMake Upgrade
echo ================================================
echo.

REM Give execute permission and run the upgrade script
wsl chmod +x /mnt/g/workspace/XOffsetDatastructure/scripts/upgrade_cmake_wsl.sh
wsl bash /mnt/g/workspace/XOffsetDatastructure/scripts/upgrade_cmake_wsl.sh

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ================================================
    echo   CMake Upgrade Successful!
    echo ================================================
    echo.
    echo Next step:
    echo   Run: wsl_build_clang_p2996.bat
    echo.
) else (
    echo.
    echo ================================================
    echo   CMake Upgrade Failed
    echo ================================================
    echo.
    echo Please check error messages and retry
    echo.
)

pause
