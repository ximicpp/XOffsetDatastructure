@echo off
chcp 65001 >nul
REM ============================================================================
REM WSL2 Environment Check and Preparation Script
REM ============================================================================

echo ================================================
echo   WSL2 Environment Check
echo ================================================
echo.

REM Check if WSL is installed
echo [1/4] Checking WSL installation status...
wsl --status >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] WSL is installed
    echo.
    
    REM Display WSL version and distributions
    echo [2/4] WSL Configuration:
    wsl --list --verbose
    echo.
    
    REM Check disk space
    echo [3/4] Checking disk space...
    wsl bash -c "df -h ~ | tail -n 1"
    echo.
    echo   Required space: 60 GB
    echo   Recommended: 80 GB+
    echo.
    
    REM Check basic tools
    echo [4/4] Checking required tools...
    wsl bash -c "which git cmake ninja" >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo   [OK] Basic tools are installed
    ) else (
        echo   [WARN] Need to install basic tools
        echo.
        echo   Run the following command to install:
        echo   wsl sudo apt update
        echo   wsl sudo apt install -y build-essential cmake ninja-build git python3
    )
    echo.
    
    echo ================================================
    echo   Environment Check Completed
    echo ================================================
    echo.
    echo Next steps:
    echo   1. Ensure sufficient disk space (60GB+)
    echo   2. Run: wsl_setup_tools.bat (install required tools)
    echo   3. Run: wsl_build_clang_p2996.bat (build Clang)
    echo.
    
) else (
    echo   [ERROR] WSL is not installed
    echo.
    echo Install WSL2:
    echo   1. Run PowerShell as Administrator
    echo   2. Execute: wsl --install
    echo   3. Restart computer
    echo   4. Set Ubuntu username and password
    echo   5. Re-run this script
    echo.
    echo Or install manually:
    echo   https://docs.microsoft.com/en-us/windows/wsl/install
    echo.
    
    set /p INSTALL="Install WSL2 now? (requires admin rights) [y/n]: "
    if /i "%INSTALL%"=="y" (
        echo.
        echo Installing WSL2...
        powershell -Command "Start-Process wsl -ArgumentList '--install' -Verb RunAs"
        echo.
        echo Installation command executed.
        echo Please wait for installation to complete, then restart your computer.
    )
)

echo.
pause
