@echo off
REM ============================================================================
REM Run Full Demo
REM ============================================================================

echo.
echo ================================================
echo   XOffsetDatastructure2 Demo
echo ================================================
echo.

wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/xoffsetdatastructure2_demo"

echo.
echo ================================================
echo   Demo Complete!
echo ================================================
pause
