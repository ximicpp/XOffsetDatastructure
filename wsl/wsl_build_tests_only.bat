@echo off
REM ============================================================================
REM Build ONLY WSL reflection test files (not the main project)
REM ============================================================================

echo.
echo ================================================
echo   Build WSL Reflection Tests Only
echo ================================================
echo.

echo Creating output directory...
if not exist wsl_tests_build mkdir wsl_tests_build
cd wsl_tests_build

echo.
echo [1/6] Building test_cpp26_simple...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_cpp26_simple.cpp -o wsl_tests_build/test_cpp26_simple -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [2/6] Building test_reflection_syntax...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_reflection_syntax.cpp -o wsl_tests_build/test_reflection_syntax -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [3/6] Building test_splice...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_splice.cpp -o wsl_tests_build/test_splice -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [4/6] Building test_reflect_syntax...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_reflect_syntax.cpp -o wsl_tests_build/test_reflect_syntax -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [5/6] Building test_reflection_final...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_reflection_final.cpp -o wsl_tests_build/test_reflection_final -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [6/8] Building test_meta_full...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_meta_full.cpp -o wsl_tests_build/test_meta_full -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [7/8] Building test_p2996_comprehensive...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_p2996_comprehensive.cpp -o wsl_tests_build/test_p2996_comprehensive -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [8/9] Building test_meta_functions...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_meta_functions.cpp -o wsl_tests_build/test_meta_functions -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

echo.
echo [9/9] Building test_advanced_meta...
%SystemRoot%\System32\wsl.exe bash -c "cd /mnt/g/workspace/XOffsetDatastructure/wsl && ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection -stdlib=libc++ test_advanced_meta.cpp -o wsl_tests_build/test_advanced_meta -L~/clang-p2996-install/lib -Wl,-rpath,~/clang-p2996-install/lib"

cd ..

echo.
echo ================================================
echo   Build Complete!
echo ================================================
echo.
echo Executables are in: wsl\wsl_tests_build\
echo.
echo To run tests:
echo   wsl_run_wsl_tests.bat
echo.
pause
