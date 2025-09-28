#!/bin/bash

set -e  # Exit on any error

echo "========================================"
echo "GAM300 vcpkg Setup Script (Linux/Mac)"
echo "========================================"
echo

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/Project"

# Change to Project directory
echo "[1/5] Changing to Project directory..."
if [ ! -d "$PROJECT_DIR" ]; then
    echo "ERROR: Could not find Project directory at $PROJECT_DIR"
    exit 1
fi
cd "$PROJECT_DIR"

echo "[2/5] Checking if vcpkg already exists..."
if [ -d "vcpkg" ]; then
    echo "vcpkg directory already exists. Removing old installation..."
    rm -rf vcpkg
fi

echo "[3/5] Cloning vcpkg repository..."
git clone https://github.com/Microsoft/vcpkg.git
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to clone vcpkg repository"
    echo "Make sure git is installed and you have internet connection"
    exit 1
fi

echo "[4/5] Bootstrapping vcpkg..."
cd vcpkg
./bootstrap-vcpkg.sh
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to bootstrap vcpkg"
    exit 1
fi

echo "[5/5] Testing vcpkg installation..."
./vcpkg version
if [ $? -ne 0 ]; then
    echo "ERROR: vcpkg installation verification failed"
    exit 1
fi

echo
echo "========================================"
echo "SUCCESS! vcpkg has been set up successfully."
echo
echo "Next steps:"
echo "1. Open VS Code or your preferred IDE"
echo "2. Configure with: cmake --preset debug (or other preset)"
echo "3. Build with: cmake --build Build/debug"
echo
echo "Note: Make sure you have the following installed:"
echo "- cmake (3.20+)"
echo "- ninja-build"
echo "- C++ compiler (gcc/clang)"
echo "========================================"
echo