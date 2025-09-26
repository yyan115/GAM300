@echo off
setlocal EnableDelayedExpansion

REM GAM300 CMake Cache Cleaner (Windows)
REM This script removes all CMake cache files and build directories
REM Usage: Run from anywhere in the GAM300 project

echo 🧹 Cleaning CMake cache for GAM300...

REM Find the project root (look for CMakeLists.txt)
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT="

REM Remove trailing backslash
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

REM Check if we're in the project root
if exist "%SCRIPT_DIR%\Project\CMakeLists.txt" (
    set "PROJECT_ROOT=%SCRIPT_DIR%\Project"
) else if exist "%SCRIPT_DIR%\CMakeLists.txt" (
    set "PROJECT_ROOT=%SCRIPT_DIR%"
) else (
    REM Search up the directory tree
    set "CURRENT_DIR=%SCRIPT_DIR%"
    :search_loop
    if exist "!CURRENT_DIR!\CMakeLists.txt" (
        set "PROJECT_ROOT=!CURRENT_DIR!"
        goto found_root
    )
    if exist "!CURRENT_DIR!\Project\CMakeLists.txt" (
        set "PROJECT_ROOT=!CURRENT_DIR!\Project"
        goto found_root
    )

    REM Go up one directory
    for %%I in ("!CURRENT_DIR!") do set "PARENT_DIR=%%~dpI"
    set "PARENT_DIR=!PARENT_DIR:~0,-1!"

    REM Check if we've reached the root
    if "!CURRENT_DIR!"=="!PARENT_DIR!" goto root_not_found

    set "CURRENT_DIR=!PARENT_DIR!"
    goto search_loop
)

:found_root
if "%PROJECT_ROOT%"=="" goto root_not_found

echo 📁 Project root: %PROJECT_ROOT%

REM Main project build directories
echo Removing main project build directories...
if exist "%PROJECT_ROOT%\Build\debug" (
    rmdir /s /q "%PROJECT_ROOT%\Build\debug"
    echo   ✓ Removed debug build
)
if exist "%PROJECT_ROOT%\Build\release" (
    rmdir /s /q "%PROJECT_ROOT%\Build\release"
    echo   ✓ Removed release build
)
if exist "%PROJECT_ROOT%\Build\editor-debug" (
    rmdir /s /q "%PROJECT_ROOT%\Build\editor-debug"
    echo   ✓ Removed editor-debug build
)
if exist "%PROJECT_ROOT%\Build\editor-release" (
    rmdir /s /q "%PROJECT_ROOT%\Build\editor-release"
    echo   ✓ Removed editor-release build
)

REM Android build directories (optional - uncomment if needed)
REM echo Removing Android build directories...
REM if exist "%PROJECT_ROOT%\Build\android-debug" (
REM     rmdir /s /q "%PROJECT_ROOT%\Build\android-debug"
REM     echo   ✓ Removed android-debug build
REM )
REM if exist "%PROJECT_ROOT%\Build\android-release" (
REM     rmdir /s /q "%PROJECT_ROOT%\Build\android-release"
REM     echo   ✓ Removed android-release build
REM )

REM Remove any stray CMake files
echo Removing any stray CMake files...
for /r "%PROJECT_ROOT%" %%F in (CMakeCache.txt) do (
    echo "%%F" | findstr /v /i "vcpkg android" >nul && del "%%F" 2>nul
)

for /r "%PROJECT_ROOT%" %%D in (CMakeFiles) do (
    echo "%%D" | findstr /v /i "vcpkg android" >nul && if exist "%%D" rmdir /s /q "%%D" 2>nul
)

for /r "%PROJECT_ROOT%" %%F in (cmake_install.cmake) do (
    echo "%%F" | findstr /v /i "vcpkg android" >nul && del "%%F" 2>nul
)

for /r "%PROJECT_ROOT%" %%F in (Makefile) do (
    echo "%%F" | findstr /v /i "vcpkg android" >nul && del "%%F" 2>nul
)

REM Clean any compiled binaries (optional)
echo Removing compiled binaries...
for /r "%PROJECT_ROOT%" %%F in (*.obj *.lib *.dll *.exp *.pdb) do (
    echo "%%F" | findstr /v /i "vcpkg" >nul && del "%%F" 2>nul
)

echo ✅ CMake cache cleanup complete!
echo.
echo You can now run:
echo   cd "%PROJECT_ROOT%"
echo   cmake -B Build/debug -S . -DCMAKE_BUILD_TYPE=Debug
echo   cmake --build Build/debug
goto end

:root_not_found
echo ❌ Error: Could not find CMakeLists.txt. Are you in the GAM300 project?
exit /b 1

:end
pause