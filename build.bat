@echo off
setlocal enabledelayedexpansion

:: Create and enter build directory
if not exist "build" mkdir build
cd build

:: Configure with CMake
echo Configuring CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

:: Build the project
echo Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

:: Run the demos
echo.
echo Running demo v1...
bin\Release\xdemo_v1.exe
echo.
echo Running demo v2...
bin\Release\xdemo_v2.exe

:: Return to original directory
cd ..

echo.
echo Build and run completed successfully!