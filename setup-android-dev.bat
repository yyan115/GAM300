@echo off
echo GAM300 Windows Development Environment Setup
echo.
echo This script will install the required tools for Android development on Windows.
echo Please run this as Administrator for best results.
echo.
pause

echo Checking for existing installations...
echo.

REM Check if winget is available
winget --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: winget is not available. Please install App Installer from Microsoft Store.
    echo    https://www.microsoft.com/store/productId/9NBLGGH4NNS1
    pause
    exit /b 1
)

echo SUCCESS: winget is available
echo.

REM Install Git (if not already installed)
echo Installing Git...
winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo SUCCESS: Git installed successfully
) else (
    echo WARNING: Git installation failed or already installed
)
echo.

REM Install Visual Studio Build Tools (required for CMake on Windows)
echo Installing Visual Studio Build Tools 2022...
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo SUCCESS: Visual Studio Build Tools installed successfully
) else (
    echo WARNING: Visual Studio Build Tools installation failed or already installed
)
echo.

REM Install CMake
echo Installing CMake...
winget install --id Kitware.CMake -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo SUCCESS: CMake installed successfully
) else (
    echo WARNING: CMake installation failed or already installed
)
echo.

REM Install Ninja
echo Installing Ninja...
winget install --id Ninja-build.Ninja -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo SUCCESS: Ninja installed successfully
) else (
    echo WARNING: Ninja installation failed or already installed
)
echo.

REM Install Android Studio
echo Installing Android Studio...
winget install --id Google.AndroidStudio -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo SUCCESS: Android Studio installed successfully
) else (
    echo WARNING: Android Studio installation failed or already installed
)
echo.

REM Install VS Code (optional but recommended)
echo Installing Visual Studio Code...
winget install --id Microsoft.VisualStudioCode -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo SUCCESS: VS Code installed successfully
) else (
    echo WARNING: VS Code installation failed or already installed
)
echo.

echo Setting up Android environment...
echo.

REM Check if Android SDK exists in common locations
set "SDK_PATH="
if exist "%LOCALAPPDATA%\Android\Sdk\ndk" (
    set "SDK_PATH=%LOCALAPPDATA%\Android\Sdk"
) else if exist "C:\Android\Sdk\ndk" (
    set "SDK_PATH=C:\Android\Sdk"
) else if exist "%USERPROFILE%\Android\Sdk\ndk" (
    set "SDK_PATH=%USERPROFILE%\Android\Sdk"
)

if "%SDK_PATH%"=="" (
    echo WARNING: Android SDK not found. Please:
    echo    1. Open Android Studio
    echo    2. Go through initial setup
    echo    3. Install Android SDK and NDK
    echo    4. Run this script again
    echo.
) else (
    echo SUCCESS: Found Android SDK: %SDK_PATH%

    REM Find the NDK version
    for /d %%i in ("%SDK_PATH%\ndk\*") do (
        set "NDK_VERSION=%%~nxi"
        set "NDK_PATH=%%i"
    )

    if "%NDK_VERSION%"=="" (
        echo WARNING: No Android NDK found. Please install NDK through Android Studio.
    ) else (
        echo SUCCESS: Found Android NDK: %NDK_VERSION%

        REM Set environment variables (using forward slashes for CMake compatibility)
        set "NDK_PATH_FORWARD=%NDK_PATH:\=/%"
        setx ANDROID_SDK_ROOT "%SDK_PATH%" >nul
        setx ANDROID_NDK_HOME "%NDK_PATH_FORWARD%" >nul

        echo SUCCESS: Environment variables set:
        echo    ANDROID_SDK_ROOT=%SDK_PATH%
        echo    ANDROID_NDK_HOME=%NDK_PATH_FORWARD%
    )
)
echo.

echo Running Android clean script for fresh start...
echo.
call "%~dp0clean-android.bat"

echo.
echo SUCCESS. Please restart your computer for best results.
@REM echo Next Steps:
@REM echo.
@REM echo 1. SUCCESS: Close and reopen your terminal/VS Code to load new environment variables
@REM echo 2. INFO: If Android SDK/NDK not found:
@REM echo    - Open Android Studio
@REM echo    - Complete initial setup wizard
@REM echo    - Install Android SDK ^& NDK through SDK Manager
@REM echo    - Run setup-android.bat to configure environment variables
@REM echo 3. INFO: Install VS Code extensions (recommended):
@REM echo    - C/C++ Extension Pack
@REM echo    - CMake Tools
@REM echo    - Android iOS Emulator
@REM echo 4. INFO: Try building: Open AndroidProject folder in Android Studio and press Run
@REM echo.
@REM echo Setup complete! Happy coding!
@REM echo.
pause