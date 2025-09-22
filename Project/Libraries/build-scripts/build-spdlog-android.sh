#!/bin/bash

# Build spdlog for Android ARM64
# This script builds spdlog as a static library for Android using make

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SPDLOG_SOURCE="$PROJECT_ROOT/spdlog"
ANDROID_OUTPUT="$PROJECT_ROOT/android/arm64"
ANDROID_HEADERS="$PROJECT_ROOT/android/headers"

# Check if Android NDK is set
if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "Error: ANDROID_NDK_HOME environment variable not set"
    echo "Please set it to your Android NDK path, e.g.:"
    echo "export ANDROID_NDK_HOME=/home/user/Android/Sdk/ndk/27.0.12077973"
    exit 1
fi

echo "Building spdlog for Android ARM64..."
echo "NDK: $ANDROID_NDK_HOME"
echo "Source: $SPDLOG_SOURCE"
echo "Output: $ANDROID_OUTPUT"

# Create build directory
BUILD_DIR="/tmp/spdlog-android-build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake for Android using make
cmake "$SPDLOG_SOURCE" \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_SYSTEM_VERSION=33 \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_ANDROID_NDK="$ANDROID_NDK_HOME" \
    -DCMAKE_ANDROID_STL_TYPE=c++_shared \
    -DCMAKE_BUILD_TYPE=Release \
    -DSPDLOG_BUILD_EXAMPLE=OFF \
    -DSPDLOG_BUILD_TESTS=OFF \
    -DSPDLOG_BUILD_BENCH=OFF \
    -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install" \
    -G "Unix Makefiles"

# Build using make
make -j$(nproc)

# Install to temp location
make install

# Copy files to our structure
mkdir -p "$ANDROID_OUTPUT"
mkdir -p "$ANDROID_HEADERS"

# Copy the static library
cp install/lib/libspdlog.a "$ANDROID_OUTPUT/"

# Copy headers
cp -r install/include/spdlog "$ANDROID_HEADERS/"

echo "✅ spdlog built successfully!"
echo "Library: $ANDROID_OUTPUT/libspdlog.a"
echo "Headers: $ANDROID_HEADERS/spdlog/"

# Clean up
cd "$PROJECT_ROOT"
rm -rf "$BUILD_DIR"