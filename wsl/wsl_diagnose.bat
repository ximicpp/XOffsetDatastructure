@echo off
REM ============================================================================
REM Check Build Environment and Prerequisites
REM ============================================================================

echo.
echo ================================================
echo   Environment Diagnostic
echo ================================================
echo.

echo [1] Checking WSL installation...
%SystemRoot%\System32\wsl.exe --status >nul 2>&1
if errorlevel 1 (
    echo [ERROR] WSL not found or not running
    goto :error
)
echo [OK] WSL is available
echo.

echo [2] Checking Clang P2996 compiler...
%SystemRoot%\System32\wsl.exe bash -c "test -f ~/clang-p2996-install/bin/clang++ && echo '[OK] Clang P2996 found' || echo '[ERROR] Clang P2996 NOT found'"
echo.

echo [3] Checking build script...
if exist build_cpp26_wsl.sh (
    echo [OK] Build script found
) else (
    echo [ERROR] build_cpp26_wsl.sh not found
)
echo.

echo [4] Checking build directory...
if exist ..\build_cpp26 (
    echo [INFO] Build directory exists
    dir ..\build_cpp26\bin 2>nul
) else (
    echo [INFO] Build directory does not exist - need to build
)
echo.

echo ================================================
echo   Diagnostic Complete
echo ================================================
echo.

echo Next steps:
echo.
echo If Clang P2996 NOT found:
echo   1. Run: wsl_setup_tools.bat
echo   2. Run: wsl_build_clang_p2996.bat
echo.
echo If Clang P2996 found:
echo   1. Run: wsl_rebuild_with_reflection.bat
echo.
pause
exit /b 0

:error
echo.
echo [ERROR] Please install WSL2 first
echo See: WSL2_QUICK_START.md
pause
exit /b 1
