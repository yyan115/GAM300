@echo off
setlocal enabledelayedexpansion

echo ========================================
echo GAM300 vcpkg Setup Script (Windows)
echo ========================================
echo.

:: Change to Project directory
cd /d "%~dp0Project"
if !errorlevel! neq 0 (
    echo ERROR: Could not find Project directory
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

echo [3/4] Bootstrapping vcpkg...
cd vcpkg
call bootstrap-vcpkg.bat
if !errorlevel! neq 0 (
    echo ERROR: Failed to bootstrap vcpkg
    pause
    exit /b 1
)

echo [4/4] Testing vcpkg installation...
vcpkg.exe version
if !errorlevel! neq 0 (
    echo ERROR: vcpkg installation verification failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! vcpkg has been set up successfully.
echo.
echo Next steps:
echo 1. Open Visual Studio or VS Code
echo 2. Configure with: cmake --preset debug (or other preset)
echo 3. Build with: cmake --build Build/debug
echo ========================================
echo.
pause