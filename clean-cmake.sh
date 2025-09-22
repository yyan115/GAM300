#!/bin/bash

# GAM300 CMake Cache Cleaner (Linux/macOS)
# This script removes all CMake cache files and build directories
# Usage: Run from anywhere in the GAM300 project

echo "🧹 Cleaning CMake cache for GAM300..."

# Find the project root (look for CMakeLists.txt)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT=""

# Check if we're in the project root
if [ -f "$SCRIPT_DIR/Project/CMakeLists.txt" ]; then
    PROJECT_ROOT="$SCRIPT_DIR/Project"
elif [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
    PROJECT_ROOT="$SCRIPT_DIR"
else
    # Search up the directory tree
    CURRENT_DIR="$SCRIPT_DIR"
    while [ "$CURRENT_DIR" != "/" ]; do
        if [ -f "$CURRENT_DIR/CMakeLists.txt" ] || [ -f "$CURRENT_DIR/Project/CMakeLists.txt" ]; then
            if [ -f "$CURRENT_DIR/Project/CMakeLists.txt" ]; then
                PROJECT_ROOT="$CURRENT_DIR/Project"
            else
                PROJECT_ROOT="$CURRENT_DIR"
            fi
            break
        fi
        CURRENT_DIR="$(dirname "$CURRENT_DIR")"
    done
fi

if [ -z "$PROJECT_ROOT" ]; then
    echo "❌ Error: Could not find CMakeLists.txt. Are you in the GAM300 project?"
    exit 1
fi

echo "📁 Project root: $PROJECT_ROOT"

# Main project build directories
echo "Removing main project build directories..."
[ -d "$PROJECT_ROOT/Build/debug" ] && rm -rf "$PROJECT_ROOT/Build/debug" && echo "  ✓ Removed debug build"
[ -d "$PROJECT_ROOT/Build/release" ] && rm -rf "$PROJECT_ROOT/Build/release" && echo "  ✓ Removed release build"
[ -d "$PROJECT_ROOT/Build/editor-debug" ] && rm -rf "$PROJECT_ROOT/Build/editor-debug" && echo "  ✓ Removed editor-debug build"
[ -d "$PROJECT_ROOT/Build/editor-release" ] && rm -rf "$PROJECT_ROOT/Build/editor-release" && echo "  ✓ Removed editor-release build"

# Android build directories (optional - uncomment if needed)
# echo "Removing Android build directories..."
# [ -d "$PROJECT_ROOT/Build/android-debug" ] && rm -rf "$PROJECT_ROOT/Build/android-debug" && echo "  ✓ Removed android-debug build"
# [ -d "$PROJECT_ROOT/Build/android-release" ] && rm -rf "$PROJECT_ROOT/Build/android-release" && echo "  ✓ Removed android-release build"

# Remove any stray CMake files in the project root
echo "Removing any stray CMake files..."
find "$PROJECT_ROOT" -maxdepth 3 -name "CMakeCache.txt" -not -path "*/vcpkg/*" -not -path "*/android-*" -delete 2>/dev/null
find "$PROJECT_ROOT" -maxdepth 3 -name "CMakeFiles" -type d -not -path "*/vcpkg/*" -not -path "*/android-*" -exec rm -rf {} + 2>/dev/null || true
find "$PROJECT_ROOT" -maxdepth 3 -name "cmake_install.cmake" -not -path "*/vcpkg/*" -not -path "*/android-*" -delete 2>/dev/null
find "$PROJECT_ROOT" -maxdepth 3 -name "Makefile" -not -path "*/vcpkg/*" -not -path "*/android-*" -delete 2>/dev/null

# Clean any compiled binaries (optional)
echo "Removing compiled binaries..."
find "$PROJECT_ROOT" -name "*.o" -delete 2>/dev/null || true
find "$PROJECT_ROOT" -name "*.a" -delete 2>/dev/null || true
find "$PROJECT_ROOT" -name "*.so" -delete 2>/dev/null || true

echo "✅ CMake cache cleanup complete!"
echo ""
echo "You can now run:"
echo "  cd \"$PROJECT_ROOT\""
echo "  cmake -B Build/debug -S . -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build Build/debug"