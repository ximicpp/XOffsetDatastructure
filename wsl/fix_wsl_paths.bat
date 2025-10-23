@echo off
REM ============================================================================
REM Fix all WSL command paths in batch files
REM ============================================================================

echo Fixing WSL command paths in all .bat files...
echo.

setlocal enabledelayedexpansion

for %%f in (*.bat) do (
    if not "%%f"=="fix_wsl_paths.bat" (
        echo Processing: %%f
        powershell -Command "(Get-Content '%%f') -replace '^wsl bash', '%%SystemRoot%%\System32\wsl.exe bash' | Set-Content '%%f.tmp'"
        move /y "%%f.tmp" "%%f" >nul
    )
)

echo.
echo Done! All WSL paths fixed.
pause
