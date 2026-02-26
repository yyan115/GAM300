@echo off
REM Registers the custom scene merge driver in your local git config.
REM Run this once after cloning the repository.

REM Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Python not found. Installing Python via winget...
    winget install Python.Python.3.12 --accept-package-agreements --accept-source-agreements
    if %errorlevel% neq 0 (
        echo.
        echo ERROR: Failed to install Python automatically.
        echo Please install Python manually from https://www.python.org/downloads/
        echo Make sure to check "Add Python to PATH" during installation.
        pause
        exit /b 1
    )
    echo.
    echo Python installed. You may need to restart your terminal for PATH to update.
    echo After restarting, run this script again.
    pause
    exit /b 0
)

git config merge.scene-merge.name "Scene smart merge"
git config merge.scene-merge.driver "python tools/scene_merge.py %%O %%A %%B %%P"

echo.
echo Scene merge driver registered successfully.
echo Git will now use semantic merging for .scene files.
pause
