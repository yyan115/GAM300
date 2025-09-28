#!/bin/bash

echo "🧹 Cleaning Android build for fresh start..."

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "📁 Deleting Project build directories..."
# Delete Android build directories
rm -rf "$SCRIPT_DIR/Project/Build/android-debug"
rm -rf "$SCRIPT_DIR/Project/Build/android-release"

echo "🗂️ Deleting Android Studio cache..."
# Delete Android Studio build cache
rm -rf "$SCRIPT_DIR/AndroidProject/app/.cxx"
rm -rf "$SCRIPT_DIR/AndroidProject/app/build"
rm -rf "$SCRIPT_DIR/AndroidProject/build"

echo "📂 Deleting Android assets Resources..."
# Delete Android assets Resources folder
rm -rf "$SCRIPT_DIR/AndroidProject/app/src/main/assets/Resources"

echo "✅ Clean complete! You can now press 'Run' in Android Studio."
echo "   Android Studio will auto-configure and copy resources fresh."