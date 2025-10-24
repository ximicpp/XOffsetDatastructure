@echo off
REM ============================================================================
REM Rebuild Project with P2996 Reflection
REM ============================================================================

echo.
echo ================================================
echo   Rebuild Project (C++26 Reflection Enabled)
echo ================================================
echo.

echo Cleaning old build...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure && rm -rf build_cpp26"

echo.
echo Starting build...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure && bash wsl/build_cpp26_wsl.sh"

echo.
echo ================================================
echo   Complete!
echo ================================================
pause
