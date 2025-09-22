# GAM300 Android Build Guide

## Overview
The GAM300 engine supports Android deployment using Android NDK and JNI bridge. The Android app loads the native Engine library and Game code.

## Prerequisites

### Required Software
- **Android Studio**: Latest version
- **Android NDK**: Version 27+ (set via Android Studio SDK Manager)
- **Android SDK**: API level 34+ (set via Android Studio SDK Manager)
- **CMake**: 3.22.1+ (available through Android Studio SDK Manager)

### Environment Setup
Add these to your `~/.bashrc` or `~/.profile`:
```bash
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_NDK_HOME="$HOME/Android/Sdk/ndk/27.0.12077973"  # Adjust version as needed
export PATH="$PATH:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin"
```

## Project Structure

```
GAM300/
├── Project/                    # Desktop Engine & Game
│   ├── Engine/
│   ├── Game/
│   └── Libraries/
│       └── android/           # Android-specific prebuilt libraries
│           ├── arm64/         # ARM64 native libraries (.so files)
│           ├── armv7/         # ARMv7 native libraries
│           └── headers/       # Android library headers
└── AndroidProject/           # Android Studio project
    └── app/
        ├── build.gradle      # Android build configuration
        └── src/main/
            ├── cpp/          # JNI bridge code
            │   ├── CMakeLists.txt
            │   └── native-lib.cpp
            └── jniLibs/      # Runtime native libraries
                └── arm64-v8a/
                    └── libassimp.so
```

## Build Process

### Step 1: Build Engine for Android

1. **Configure Android build:**
   ```bash
   cd Project
   cmake --preset android-debug
   ```

2. **Build Engine library:**
   ```bash
   cmake --build Build/android-debug
   ```

   This creates `Build/android-debug/lib/libEngine.so`

### Step 2: Prepare Android Dependencies

The Android project uses prebuilt libraries located in `Project/Libraries/android/`:

- **Headers**: `headers/` (GLM, RapidJSON, Assimp, spdlog, FreeType)
- **ARM64 Libraries**: `arm64/` (libassimp.so, etc.)
- **Runtime Libraries**: Copied to `AndroidProject/app/src/main/jniLibs/arm64-v8a/`

### Step 3: Build Android APK

1. **Open Android Studio:**
   ```bash
   cd AndroidProject
   # Open this directory in Android Studio
   ```

2. **Or build via command line:**
   ```bash
   cd AndroidProject
   ./gradlew assembleDebug
   ```

3. **Install to device/emulator:**
   ```bash
   ./gradlew installDebug
   ```

### Step 4: Deploy to Emulator

1. **Start emulator** (if needed):
   ```bash
   cd ~/Android/Sdk/emulator
   ./emulator -avd YOUR_AVD_NAME
   ```

2. **Install APK:**
   ```bash
   adb install AndroidProject/app/build/outputs/apk/debug/app-debug.apk
   ```

3. **View logs:**
   ```bash
   adb logcat | grep GAM300
   ```

## Android-Specific Configuration

### build.gradle Settings
- **Target SDK**: 36
- **Min SDK**: 34
- **ABI Filter**: arm64-v8a (modern 64-bit ARM)
- **STL**: c++_shared (for C++ standard library)

### CMake Configuration
- **Engine Library**: Links to prebuilt `libEngine.so` from desktop build
- **Game Source**: Includes Game/*.cpp (except main.cpp)
- **JNI Bridge**: `native-lib.cpp` handles Android lifecycle

### Asset Loading (Known Issue)
- **Current Status**: App runs but shows black screen
- **Cause**: Asset loading expects desktop "Resources" folder, but Android uses "assets" folder
- **Solution Needed**: Abstract asset loading to handle both desktop and Android paths

## Troubleshooting

### Common Issues

1. **Engine library not found:**
   - Ensure `cmake --build Build/android-debug` completed successfully
   - Check that `Build/android-debug/lib/libEngine.so` exists

2. **Missing dependencies:**
   - Verify `libassimp.so` is in `AndroidProject/app/src/main/jniLibs/arm64-v8a/`
   - Check that `libc++_shared.so` is available (handled by STL setting)

3. **Black screen on startup:**
   - This is the current known issue
   - App runs but cannot load assets from desktop "Resources" path
   - Need to implement Android asset manager integration

### Debug Commands
```bash
# Check what libraries are included in APK
unzip -l app-debug.apk | grep "\.so"

# Monitor Android logs
adb logcat | grep -E "(GAM300|AndroidRuntime|DEBUG)"

# Check for missing libraries
adb logcat | grep "dlopen failed"
```

## Development Workflow

1. **Make changes** to Engine/Game code in desktop project
2. **Build for Android**: `cmake --build Build/android-debug`
3. **Build Android APK**: `./gradlew assembleDebug`
4. **Install and test**: `./gradlew installDebug`

## Next Steps

**Priority**: Fix asset loading to resolve black screen issue
- Implement Android asset manager in Engine
- Abstract file I/O to handle both desktop and Android paths
- Update Resource loading to use Android "assets" folder

The Android build infrastructure is working - the core issue is asset path abstraction.