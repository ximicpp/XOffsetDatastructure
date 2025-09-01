@echo off
setlocal enabledelayedexpansion

:: Create and enter build directory
if not exist "build" mkdir build
cd build

:: Configure with CMake
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

:: Build the project
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

:: Run the demos
echo.
echo Running demo...
bin\Release\xdemo.exe
echo.
echo Running demov2...
bin\Release\xdemov2.exe

cd ..