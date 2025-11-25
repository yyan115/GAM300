@echo off
echo ========================================
echo   WARNING: Close Visual Studio first!
echo ========================================
echo.
echo This script will delete:
echo   - Build/
echo   - .vs/
echo.
set /p confirm=Are you sure? HAVE YOU CLOSED VISUAL STUDIO? (y/n):
if /i not "%confirm%"=="y" (
    echo Cancelled.
    pause
    exit /b
)

echo.
echo Cleaning build files...

if exist "Build" (
    echo Deleting Build/
    rmdir /s /q "Build"
)

if exist ".vs" (
    echo Deleting .vs/
    rmdir /s /q ".vs"
)

echo.
echo Done! Reopen the Project folder in Visual Studio.
pause
