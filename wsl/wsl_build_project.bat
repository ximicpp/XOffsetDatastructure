@echo off
chcp 65001 >nul
REM ============================================================================
REM Compile XOffsetDatastructure2 (C++26) in WSL2
REM ============================================================================

echo ================================================
echo   Compile XOffsetDatastructure2 (C++26)
echo ================================================
echo.

echo Compiling project using Clang P2996 in WSL2
echo Make sure Clang P2996 build is completed
echo.

set /p CONTINUE="Continue? [y/n]: "
if /i not "%CONTINUE%"=="y" (
    echo Cancelled
    exit /b 0
)

echo.
echo Starting compilation...
echo.

wsl bash ./build_cpp26_wsl.sh

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ================================================
    echo   Compilation Successful!
    echo ================================================
    echo.
    echo Run tests (in WSL):
    echo   wsl ./build_cpp26/bin/helloworld
    echo   wsl ./build_cpp26/bin/test_compaction
    echo   wsl ./build_cpp26/bin/xoffsetdatastructure2_demo
    echo.
    echo Or enter WSL interactive environment:
    echo   wsl
    echo   cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26
    echo   ./bin/helloworld
    echo.
) else (
    echo.
    echo ================================================
    echo   Compilation Failed
    echo ================================================
    echo.
    echo Please check error messages
    echo.
)

pause
