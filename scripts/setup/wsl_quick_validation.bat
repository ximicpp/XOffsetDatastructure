@echo off
chcp 65001 >nul
REM ============================================================================
REM Quick Validation Script - Run before long build
REM ============================================================================

echo ================================================
echo   WSL2 Environment Quick Validation
echo ================================================
echo.
echo This script will validate all prerequisites
echo Estimated time: 2-3 minutes
echo.

set ERRORS=0

REM ============================================================================
REM Test 1: WSL Availability
REM ============================================================================

echo [1/8] Testing WSL availability...
wsl echo "WSL is working" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] WSL is available
) else (
    echo   [ERROR] WSL is not available or not installed
    set /a ERRORS+=1
)
echo.

REM ============================================================================
REM Test 2: Disk Space
REM ============================================================================

echo [2/8] Checking disk space...
wsl bash -c "df -h ~ | tail -1 | awk '{print \$4}'"
echo   [INFO] Check if you have 60GB+ available
echo.

REM ============================================================================
REM Test 3: Required Tools
REM ============================================================================

echo [3/8] Checking required tools...
wsl bash -c "which git cmake ninja gcc g++ 2>&1 | grep -q '/bin/' && echo 'found' || echo 'missing'" > %TEMP%\tool_check.txt
findstr "found" %TEMP%\tool_check.txt >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] Basic tools are installed
) else (
    echo   [ERROR] Missing required tools
    echo   Please run: wsl_setup_tools.bat
    set /a ERRORS+=1
)
del %TEMP%\tool_check.txt >nul 2>&1
echo.

REM ============================================================================
REM Test 4: Network Connection
REM ============================================================================

echo [4/8] Testing network connection...
wsl timeout 5 git ls-remote https://github.com/bloomberg/clang-p2996.git HEAD >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] GitHub is accessible
) else (
    echo   [WARN] GitHub is slow or unreachable
    echo   This may affect clone speed
)
echo.

REM ============================================================================
REM Test 5: Project Path
REM ============================================================================

echo [5/8] Checking project path...
wsl bash -c "test -d /mnt/g/workspace/XOffsetDatastructure && echo 'ok' || echo 'fail'" > %TEMP%\path_check.txt
findstr "ok" %TEMP%\path_check.txt >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] Project path is accessible
) else (
    echo   [ERROR] Project path not accessible: /mnt/g/workspace/XOffsetDatastructure
    set /a ERRORS+=1
)
del %TEMP%\path_check.txt >nul 2>&1
echo.

REM ============================================================================
REM Test 6: Boost Headers
REM ============================================================================

echo [6/8] Checking Boost headers...
wsl bash -c "test -d /mnt/g/workspace/XOffsetDatastructure/external/boost && echo 'ok' || echo 'fail'" > %TEMP%\boost_check.txt
findstr "ok" %TEMP%\boost_check.txt >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] Boost headers exist
) else (
    echo   [ERROR] Boost headers not found
    echo   Please ensure external/boost directory exists
    set /a ERRORS+=1
)
del %TEMP%\boost_check.txt >nul 2>&1
echo.

REM ============================================================================
REM Test 7: Compile Environment
REM ============================================================================

echo [7/8] Testing compile environment...
wsl bash -c "echo 'int main(){return 0;}' > /tmp/test_c.cpp && g++ /tmp/test_c.cpp -o /tmp/test_c 2>&1 && /tmp/test_c && echo 'ok' || echo 'fail'" > %TEMP%\compile_check.txt
findstr "ok" %TEMP%\compile_check.txt >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] Compile environment is normal
) else (
    echo   [ERROR] Compile environment has issues
    set /a ERRORS+=1
)
wsl rm -f /tmp/test_c.cpp /tmp/test_c >nul 2>&1
del %TEMP%\compile_check.txt >nul 2>&1
echo.

REM ============================================================================
REM Test 8: Script Syntax
REM ============================================================================

echo [8/8] Checking script syntax...
wsl bash -n ./build_cpp26_wsl.sh >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   [OK] Script syntax is correct
) else (
    echo   [WARN] Script syntax check failed
    echo   This may not affect execution
)
echo.

REM ============================================================================
REM Summary
REM ============================================================================

echo ================================================
echo   Validation Summary
echo ================================================
echo.

if %ERRORS% EQU 0 (
    echo [SUCCESS] All checks passed!
    echo.
    echo You can safely start building:
    echo   1. Run: wsl_build_clang_p2996.bat
    echo   2. Estimated time: 1-3 hours
    echo   3. You can do other things, but don't close the terminal
    echo.
) else (
    echo [ERROR] Found %ERRORS% issue(s)
    echo.
    echo Please fix the above issues before building
    echo.
    echo Common solutions:
    echo   - Missing tools: run wsl_setup_tools.bat
    echo   - Path error: check project location
    echo   - WSL issue: run wsl --update
    echo.
)

echo Detailed docs: docs\SCRIPT_VALIDATION.md
echo.

pause
