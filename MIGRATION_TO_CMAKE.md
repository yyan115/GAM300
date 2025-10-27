# Migration to CMake-only Build System

## Overview

The GAM300 project has been fully migrated from a dual build system (Visual Studio .sln + CMake) to a **unified CMake-only build system**. This means:

- ✅ **No more .sln or .vcxproj files needed**
- ✅ **Visual Studio 2022 can open the project directly via CMake**
- ✅ **Unified build system across Windows, Linux, and Android**
- ✅ **All dependencies managed through vcpkg (desktop) or FetchContent (Android)**

## What Changed

### Old System (Before Migration)
- **Windows**: Visual Studio .sln with precompiled binaries + partial vcpkg
- **Linux**: CMake (partially working, outdated)
- **Android**: CMake via Android Studio
- **Jolt Physics**: FetchContent for all platforms

### New System (After Migration)
- **Windows**: CMake + vcpkg for ALL dependencies (including Jolt!)
- **Linux**: CMake + vcpkg for ALL dependencies
- **Android**: CMake + FetchContent (Jolt) + prebuilt binaries (others)
- **Visual Studio 2022**: Opens project via `CMakePresets.json`

## Prerequisites

### 1. Install vcpkg

```bash
# Run the existing setup script
./setup-vcpkg.bat
```

This will install vcpkg and all required dependencies from `vcpkg.json`:
- assimp
- glfw3
- freetype
- glad
- glm
- spdlog
- rapidjson
- gli
- joltphysics v5.4.0

### 2. Install Ninja (for faster builds)

Download from: https://ninja-build.org/

Or via winget:
```bash
winget install Ninja-build.Ninja
```

### 3. Install Visual Studio 2022 (with CMake support)

Make sure you have:
- **Desktop development with C++** workload
- **CMake tools for Windows** component

## How to Build

### Option 1: Visual Studio 2022 (Recommended)

1. **Open the project**:
   - File → Open → Folder
   - Navigate to `GAM300/Project/`
   - Visual Studio will automatically detect `CMakeLists.txt` and `CMakePresets.json`

2. **Select configuration**:
   - Use the configuration dropdown in the toolbar
   - Available configurations:
     - **Debug** - Game executable (standalone)
     - **Release** - Game executable (standalone)
     - **Editor Debug** - Editor with Game as static library
     - **Editor Release** - Editor with Game as static library

3. **Build**:
   - Build → Build All (Ctrl+Shift+B)
   - Output will be in `Project/Build/<Configuration>/`

4. **Run**:
   - Select startup item dropdown → Choose `Game.exe` or `Editor.exe`
   - Debug → Start Debugging (F5) or Start Without Debugging (Ctrl+F5)

### Option 2: Command Line (CMake)

```bash
cd Project

# Configure
cmake --preset debug  # or release, editor-debug, editor-release

# Build
cmake --build Build/cmake-debug

# Run
./Build/Debug/Game.exe       # Game
./Build/EditorDebug/Editor.exe  # Editor
```

### Option 3: VS Code

1. Install extensions:
   - CMake Tools
   - C/C++ Extension Pack

2. Open `Project/` folder in VS Code

3. Select kit and configuration using CMake extension

4. Build and run using CMake Tools buttons

## Build Configurations

| Configuration | Engine | Game | Editor | Use Case |
|--------------|--------|------|--------|----------|
| **Debug** | Shared | Executable | ❌ Not built | Development - Standalone game |
| **Release** | Shared | Executable | ❌ Not built | Production - Standalone game |
| **EditorDebug** | Shared | Static | Executable | Development - Editor + Game |
| **EditorRelease** | Shared | Static | Executable | Production - Editor + Game |

## Output Directories

All build outputs go to:
```
Project/Build/
├── Debug/              # Game exe + DLLs
├── Release/            # Game exe + DLLs
├── EditorDebug/        # Editor exe + DLLs + Game.lib
├── EditorRelease/      # Editor exe + DLLs + Game.lib
└── Intermediate/       # Object files (.obj, .a)
```

## C++ Standards

- **Engine**: C++20
- **Game**: C++17 (matches existing codebase)
- **Editor**: C++20

## Dependencies

### Desktop (Windows/Linux) - vcpkg

All managed automatically via `vcpkg.json`:
- assimp
- glfw3
- freetype
- glad
- glm
- spdlog
- rapidjson
- gli
- **joltphysics v5.4.0** ⭐ Now from vcpkg!

### Manual Libraries (Not in vcpkg)

Located in `Project/Libraries/`:
- **FMOD** - Commercial audio library (prebuilt)
- **ImGui** - UI library (docking branch, compiled from source)
- **IconFontCppHeaders** - Icon fonts for editor
- **FileWatch** - File system monitoring (editor only)
- **Compressonator** - Texture compression (editor only)

### Android - FetchContent + Prebuilt

- **FetchContent**: Jolt Physics v5.4.0
- **Prebuilt**: spdlog, freetype, assimp, FMOD

## Preprocessor Definitions

### Engine
- `ENGINE_EXPORTS` - Always defined when building Engine DLL
- `_DEBUG` / `NDEBUG` - Based on configuration
- `EDITOR` / `EDITOR=1` - Only in Editor configurations
- `JPH_DEBUG_RENDERER` - Always enabled for Jolt debug rendering
- `JPH_PROFILE_ENABLED` - Enabled in Debug configurations

### Game
- `_DEBUG` / `NDEBUG` - Based on configuration
- `_CONSOLE` - Windows console app
- `EDITOR` / `EDITOR=1` - Only in Editor configurations
- `JPH_PROFILE_ENABLED` - Enabled in Debug configurations

### Editor
- `_DEBUG` / `NDEBUG` - Based on configuration
- `_CONSOLE` - Windows console app
- `EDITOR` - Always defined for editor builds
- `JPH_PROFILE_ENABLED` - Enabled in Debug configurations

## Precompiled Headers

**Engine only** uses precompiled headers (`pch.h`) for faster compilation:
- Automatically configured in MSVC builds
- `glad.c` is excluded from PCH

## Known Issues & Troubleshooting

### Issue: vcpkg not found
```
Solution: Run setup-vcpkg.bat or set VCPKG_ROOT environment variable
```

### Issue: Jolt Physics not found
```
Solution: Delete vcpkg_installed/ folder and re-run vcpkg install:
  cd Project
  rmdir /s /q vcpkg_installed
  vcpkg install
```

### Issue: FMOD not found
```
Solution: Check that FMOD libraries exist in:
  Windows: Project/Libraries/FMOD/core/lib/x64/fmod_vc.lib
  Linux: Project/Libraries/FMOD/core/lib/x64/libfmod.so.14.9
```

### Issue: assimp DLL not found at runtime
```
Solution: The DLL should be auto-copied by CMake post-build command.
If it fails, manually copy from Libraries/Assimp/bin/x64/ to build output.
```

### Issue: Visual Studio can't find CMake
```
Solution:
1. Open Visual Studio Installer
2. Modify your installation
3. Ensure "CMake tools for Windows" is installed
4. Restart Visual Studio
```

## Android Development

Android Studio still uses the same CMake system. Nothing changes:

1. Open `AndroidProject/` in Android Studio
2. Select device/emulator
3. Run (Shift+F10)

Android uses the same main CMakeLists.txt but:
- Uses prebuilt libraries instead of vcpkg
- Uses FetchContent for Jolt Physics
- Handled automatically by `ImportDependencies.cmake`

## Migrating Your Workflow

### If you were using Visual Studio .sln:

**Old**:
1. Double-click `GAM300.sln`
2. Select configuration (Debug, Release, EditorDebug, EditorRelease)
3. Set startup project (Game or Editor)
4. Build and run

**New**:
1. Open Folder → `Project/`
2. Select preset (debug, release, editor-debug, editor-release)
3. Select startup item (Game.exe or Editor.exe)
4. Build and run

### If you were using VS Code:

Nothing changes! VS Code already used CMake.

## Benefits of Migration

✅ **Single source of truth** - One build system for all platforms
✅ **Easier onboarding** - New developers only learn CMake
✅ **Consistent dependencies** - vcpkg ensures same versions everywhere
✅ **Better IDE support** - Modern IDEs prefer CMake
✅ **Simpler CI/CD** - One build script for all platforms
✅ **No more .sln drift** - CMake auto-generates project files

## Cleaning Build Artifacts

To start fresh:

```bash
cd Project

# Delete CMake build directories
rmdir /s /q Build/cmake-*

# Delete vcpkg installed packages (forces reinstall)
rmdir /s /q vcpkg_installed

# Rebuild
cmake --preset editor-debug
cmake --build Build/cmake-editor-debug
```

## Next Steps

1. ✅ Remove old .sln and .vcxproj files (they're no longer needed!)
2. ✅ Update documentation to mention CMake-only workflow
3. ✅ Update CI/CD pipelines to use CMake
4. ✅ Consider migrating Android to FetchContent for ALL libraries (future improvement)

## Questions?

- **CMake docs**: https://cmake.org/documentation/
- **vcpkg docs**: https://vcpkg.io/
- **Visual Studio CMake**: https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio

---

**Migration completed by**: Claude Code
**Date**: 2025-10-27
**CMake version**: 3.20+
**vcpkg baseline**: 62efe42f53b1886a20cbeb22ee9a27736d20f149
