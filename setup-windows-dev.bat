@echo off
echo 🚀 GAM300 Windows Development Environment Setup
echo.
echo This script will install the required tools for Android development on Windows.
echo Please run this as Administrator for best results.
echo.
pause

echo 📋 Checking for existing installations...
echo.

REM Check if winget is available
winget --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ winget is not available. Please install App Installer from Microsoft Store.
    echo    https://www.microsoft.com/store/productId/9NBLGGH4NNS1
    pause
    exit /b 1
)

echo ✅ winget is available
echo.

REM Install Git (if not already installed)
echo 📦 Installing Git...
winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo ✅ Git installed successfully
) else (
    echo ⚠️  Git installation failed or already installed
)
echo.

REM Install Visual Studio Build Tools (required for CMake on Windows)
echo 📦 Installing Visual Studio Build Tools 2022...
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo ✅ Visual Studio Build Tools installed successfully
) else (
    echo ⚠️  Visual Studio Build Tools installation failed or already installed
)
echo.

REM Install CMake
echo 📦 Installing CMake...
winget install --id Kitware.CMake -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo ✅ CMake installed successfully
) else (
    echo ⚠️  CMake installation failed or already installed
)
echo.

REM Install Ninja
echo 📦 Installing Ninja...
winget install --id Ninja-build.Ninja -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo ✅ Ninja installed successfully
) else (
    echo ⚠️  Ninja installation failed or already installed
)
echo.

REM Install Android Studio
echo 📦 Installing Android Studio...
winget install --id Google.AndroidStudio -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo ✅ Android Studio installed successfully
) else (
    echo ⚠️  Android Studio installation failed or already installed
)
echo.

REM Install VS Code (optional but recommended)
echo 📦 Installing Visual Studio Code...
winget install --id Microsoft.VisualStudioCode -e --silent --accept-package-agreements --accept-source-agreements
if %errorlevel% equ 0 (
    echo ✅ VS Code installed successfully
) else (
    echo ⚠️  VS Code installation failed or already installed
)
echo.

echo 🔧 Setting up Android environment...
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
    echo ⚠️  Android SDK not found. Please:
    echo    1. Open Android Studio
    echo    2. Go through initial setup
    echo    3. Install Android SDK and NDK
    echo    4. Run this script again
    echo.
) else (
    echo ✅ Found Android SDK: %SDK_PATH%

    REM Find the NDK version
    for /d %%i in ("%SDK_PATH%\ndk\*") do (
        set "NDK_VERSION=%%~nxi"
        set "NDK_PATH=%%i"
    )

    if "%NDK_VERSION%"=="" (
        echo ⚠️  No Android NDK found. Please install NDK through Android Studio.
    ) else (
        echo ✅ Found Android NDK: %NDK_VERSION%

        REM Set environment variables (using forward slashes for CMake compatibility)
        set "NDK_PATH_FORWARD=%NDK_PATH:\=/%"
        setx ANDROID_SDK_ROOT "%SDK_PATH%" >nul
        setx ANDROID_NDK_HOME "%NDK_PATH_FORWARD%" >nul

        echo ✅ Environment variables set:
        echo    ANDROID_SDK_ROOT=%SDK_PATH%
        echo    ANDROID_NDK_HOME=%NDK_PATH_FORWARD%
    )
)
echo.

echo 📝 Next Steps:
echo.
echo 1. ✅ Close and reopen your terminal/VS Code to load new environment variables
echo 2. 📱 If Android SDK/NDK not found:
echo    - Open Android Studio
echo    - Complete initial setup wizard
echo    - Install Android SDK ^& NDK through SDK Manager
echo    - Run setup-android.bat to configure environment variables
echo 3. 🔧 Install VS Code extensions (recommended):
echo    - C/C++ Extension Pack
echo    - CMake Tools
echo    - Android iOS Emulator
echo 4. 🚀 Try building: Open AndroidProject folder in Android Studio and press Run
echo.
echo 🎉 Setup complete! Happy coding!
echo.
pause