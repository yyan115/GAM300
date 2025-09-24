@echo off
echo 🧹 Cleaning Android build for fresh start...

REM Get script directory
set SCRIPT_DIR=%~dp0

echo 📁 Deleting Project build directories...
REM Delete Android build directories
if exist "%SCRIPT_DIR%Project\Build\android-debug" rmdir /s /q "%SCRIPT_DIR%Project\Build\android-debug"
if exist "%SCRIPT_DIR%Project\Build\android-release" rmdir /s /q "%SCRIPT_DIR%Project\Build\android-release"

echo 🗂️ Deleting Android Studio cache...
REM Delete Android Studio build cache
if exist "%SCRIPT_DIR%AndroidProject\app\.cxx" rmdir /s /q "%SCRIPT_DIR%AndroidProject\app\.cxx"
if exist "%SCRIPT_DIR%AndroidProject\app\build" rmdir /s /q "%SCRIPT_DIR%AndroidProject\app\build"
if exist "%SCRIPT_DIR%AndroidProject\build" rmdir /s /q "%SCRIPT_DIR%AndroidProject\build"

echo 📂 Deleting Android assets Resources...
REM Delete Android assets Resources folder
if exist "%SCRIPT_DIR%AndroidProject\app\src\main\assets\Resources" rmdir /s /q "%SCRIPT_DIR%AndroidProject\app\src\main\assets\Resources"

echo ✅ Clean complete! You can now press 'Run' in Android Studio.
echo    Android Studio will auto-configure and copy resources fresh.
pause