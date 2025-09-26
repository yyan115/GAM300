@echo off
echo Setting up Android development environment...

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
    echo ERROR: Android SDK not found in common locations:
    echo   - %LOCALAPPDATA%\Android\Sdk
    echo   - C:\Android\Sdk
    echo   - %USERPROFILE%\Android\Sdk
    echo.
    echo Please install Android Studio or set ANDROID_SDK_ROOT manually.
    pause
    exit /b 1
)

REM Find the NDK version
for /d %%i in ("%SDK_PATH%\ndk\*") do (
    set "NDK_VERSION=%%~nxi"
    set "NDK_PATH=%%i"
)

if "%NDK_VERSION%"=="" (
    echo ERROR: No Android NDK found in %SDK_PATH%\ndk\
    echo Please install the Android NDK through Android Studio.
    pause
    exit /b 1
)

echo Found Android SDK: %SDK_PATH%
echo Found Android NDK: %NDK_VERSION%

echo Setting environment now. DO NOT CLOSE. It can take up to 2-5 minutes.

REM Set environment variables (using forward slashes for CMake compatibility)
set "NDK_PATH_FORWARD=%NDK_PATH:\=/%"
setx ANDROID_SDK_ROOT "%SDK_PATH%" >nul
setx ANDROID_NDK_HOME "%NDK_PATH_FORWARD%" >nul

echo.
echo Environment variables set:
echo   ANDROID_SDK_ROOT=%SDK_PATH%
echo   ANDROID_NDK_HOME=%NDK_PATH_FORWARD%
echo.
echo Setup complete! Please restart your terminal/VS Code to use the new environment variables.
echo.
pause