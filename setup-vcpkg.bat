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

echo [4/4] Integrating vcpkg with Visual Studio...
vcpkg.exe install
if !errorlevel! neq 0 (
    echo ERROR: Failed to integrate vcpkg with Visual Studio
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! vcpkg has been set up successfully.
echo.
echo vcpkg is now integrated with Visual Studio.
echo When you open the solution in Visual Studio, it will
echo automatically install dependencies from vcpkg.json
echo ========================================
echo.
pause