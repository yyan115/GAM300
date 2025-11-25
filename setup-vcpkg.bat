@echo off
setlocal enabledelayedexpansion

echo ========================================
echo GAM300 vcpkg Setup Script (Windows)
echo ========================================
echo.

:: Stay in GAM300 directory (where this script is)
cd /d "%~dp0"
if !errorlevel! neq 0 (
    echo ERROR: Could not change to script directory
    pause
    exit /b 1
)

echo [1/4] Checking if vcpkg already exists...
if exist "vcpkg\" (
    echo vcpkg directory already exists. Removing old installation...
    rmdir /s /q vcpkg
    if !errorlevel! neq 0 (
        echo ERROR: Failed to remove existing vcpkg directory
        pause
        exit /b 1
    )
)

echo [2/4] Cloning vcpkg repository...
git clone https://github.com/Microsoft/vcpkg.git
if !errorlevel! neq 0 (
    echo ERROR: Failed to clone vcpkg repository
    echo Make sure git is installed and you have internet connection
    pause
    exit /b 1
)

echo [2.5/4] Checking out pinned vcpkg version (2d6a6cf3ac - Nov 13, 2025)...
cd vcpkg
git checkout 2d6a6cf3ac9a7cc93942c3d289a2f9c661a6f4a7
if !errorlevel! neq 0 (
    echo ERROR: Failed to checkout vcpkg baseline commit
    pause
    exit /b 1
)
cd ..

echo [3/3] Bootstrapping vcpkg...
cd vcpkg
call bootstrap-vcpkg.bat -disableMetrics
if !errorlevel! neq 0 (
    echo ERROR: Failed to bootstrap vcpkg
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! vcpkg has been set up successfully.
echo.
echo CMake will automatically use vcpkg via the toolchain file.
echo Open Project folder in Visual Studio to start building.
echo ========================================
echo.
pause