# GAM300

GAM300 - Team Marbles.

Custom 3D game engine written in C++. Supports cross platform for Windows, Linux, and android.

**Live Website:** https://yyan115.github.io/GAM300/

Our engine is called Engine Engine, because we have great naming sense.

## Quick Start

Run setup-vcpkg.bat and setup-android-dev.bat to set up all development requirements for Windows, Linux and android.

1. Open Visual Studio, press "open a local folder", select "Project" folder and open it
2. Let cmake set up and config everything for you. First run will take 5-10mins. You can see it configure in the output console in Visual Studio. Wait for it to say "1> CMake generation finished."
3. Select your compile option and startup item (green button there), its either gonna be Editor.exe or Game.exe. EDITOR MUST BE RAN FIRST TO COMPILE ASSETS BEFORE GAME CAN BE RAN.

There is no solution files anymore as this is the modern way of using cmake with Visual Studios.

## Project Structure

The project supports using either Visual Studio or Visual Studio Code. Android Studio is required for Android development.

The project is separated into 3 smaller project - Engine, Game, and Edtior.

Game Assets such as fonts, shaders, sprites, etc are stored in Project/Resources. This resources folder gets copied to the built binary folder location during compilation for the game to access.

### Engine Architecture

The engine always get compiled into a dynamic library for the game to use.

For game developers, simply include "Engine.h" to start developing.

The engine ideally does not expose any other APIs other than Engine.h (E.G. The game developers should not know GraphicsManager exists and should not have access to it), however due to development time constraints and issues, some APIs might be exposed here and there.

### Game Architecture

The game can be compiled with either "Debug" or "Release" options.

Include "Engine.h" to start developing.

Scripting is WIP.

### Editor Architecture

The editor can be compiled with either "EditorDebug" or "EditorRelease" options.

The game gets built into a static library for the editor to use.

The editor uses IMGUI, and does not get compiled when game is the built target (so that we do not ship the editor).

### Libraries Used

The libraries used as of 2025-09-28:
- ASSIMP 
- Filewatch (Desktop only)
- FMOD
- Freetype
- GLFW (Desktop only)
- GLAD (Desktop only)
- GLI
- GLM
- Rapidjson
- Spdlog

Libraries used for editor only:
- IconFontCppHeaders
- IMGUI

Libraries are rather messy as some needs to manually compiled, some have available binaries for download, and some are availabe in vcpkg. They will be pointed out in the Visual Studio, Visual Studio Code, and Android development sections.

## Set up Development Environment

Visual Studio Code IS SLOW TO UPDATE, MAY BREAK, AND IS NOT RECOMMENDED. Please use Visual Studio instead.

Everyone should set up Android Studio to make sure their code works on Android.

Some additional library set ups may be required but not covered as everyone should already be familiar with them (E.G. GLFW, git). If possible, I will write a more comprehensive guide next time, as I currently do not know which exact libraries I need to point out.

### Visual Studios

1. Run setup-vcpkg.bat
2. Open Visual Studio, press "open a local folder", select "Project" folder and open it
3. Let cmake set up and config everything for you. First run will take 5-10mins. You can see it configure in the output console in Visual Studio. Wait for it to say "1> CMake generation finished."
4. Select your compile option and startup item (green button there), its either gonna be Editor.exe or Game.exe

### Visual Studio Code

Visual Studio Code uses CMake and Ninja to compile and run.

Install Visual Studio Code and install official extensions for C++ and Cmake.

Conveniently, "setup-android-dev.bat" also helps to install tools for visual studio code. So please run that. More specifically, it installs cmake and ninja for you, if you'd like to install that yourself.

Lastly, run setup-vcpkg.bat to install and setup vcpkg.

You can then choose the compile option in Visual Studio Code to compile and run.

Visual Studio Code primarily uses vcpkg to source libraries. However, some libraries are not available inside, such as FMOD, so they have to be downloaded instead.

### Linux

Linux uses Visual Studio Code with CMake Tools, CMake, Ninja, and vcpkg.
Windows still uses the existing Visual Studio folder workflow with `Project/CMakePresets.json`.

Linux presets live in `Project/CMakeUserPresets.json` so the existing Windows preset file does not need to change.
CMake automatically reads both files when you run commands from `Project`.

Supported Linux host status: Fedora is the only validated Linux development host right now. Other distributions may work, but their package names have not been verified yet.

Install system tools first. On Fedora:

```bash
sudo dnf install -y git git-lfs cmake ninja-build gcc-c++ make zip unzip tar pkgconf-pkg-config libX11-devel libXcursor-devel libXinerama-devel libXrandr-devel libXi-devel libXext-devel libXrender-devel libXfixes-devel libXxf86vm-devel mesa-libGLU-devel
```

For other Linux distributions, install equivalent packages for: Git, Git LFS, CMake, Ninja, a C++ compiler, archive tools, pkg-config, X11 development headers/libraries, and GLU development headers/libraries.

Then set up vcpkg and build:

```bash
./setup-vcpkg.sh
cd Project
cmake --preset linux-editor-debug
cmake --build --preset linux-editor-debug
```

Available Linux presets:

- `linux-editor-debug`
- `linux-editor-release`
- `linux-debug`
- `linux-release`

Run the editor first to compile assets before running the game, same as Windows.

## Android Development

First install Android Studio. Go through the initial setup.

Then, run "setup-android-dev.bat" to install required tools for Android.

Once that is done, ideally you should restart your computer for best results. Then, open Android Studio and press run. If you get any errors, see section for frequent errors for Android Studio.

Android depends on Visual Studio code to compile, which is why you should have Visual Studio Code installed. You do not need Visual Studio Code open to run for Android, as I've configured the gradle and cmake so that you only need to open Android Studio to run for Android.

The Run button in Android Studio builds the debug variant. The engine library for that variant is compiled as RelWithDebInfo (optimized, with symbols) so that frame rates seen while running from Android Studio match the release APK. Only the small JNI bridge is a true debug build. If you need to step through engine code at -O0, set the CMake cache entry `GAM300_ANDROID_DEBUG_ENGINE_CONFIG` to `Debug` in `AndroidProject/app/src/main/cpp/CMakeLists.txt` (or pass it through gradle's cmake arguments) and run clean-android.bat. Engine builds live in `Project/Build/android-relwithdebinfo` and `Project/Build/android-release`.

### Android render scale

Android renders the 3D scene at native resolution by default, the same as desktop. If a phone is GPU-bound, the CMake cache entries `GAM300_ANDROID_RENDER_SCALE` (maximum internal scale, e.g. `0.75`) and `GAM300_ANDROID_MIN_RENDER_SCALE` (floor the runtime may drop to after sustained missed frame deadlines, e.g. `0.5`) in the engine's CMakeLists trade sharpness for fill rate; the image is upscaled to the native surface during tone mapping. To change them for the Android build, add `-DGAM300_ANDROID_RENDER_SCALE=0.75` style arguments to the engine configure call in `AndroidProject/app/src/main/cpp/CMakeLists.txt` (next to `GAM300_FRAME_STATS`) or edit the defaults in `Project/Engine/CMakeLists.txt`, then run clean-android.bat so the cached values are replaced.

Texture resolution is a separate knob. The Android package currently holds non-UI textures at 1024 pixels maximum (the desktop package keeps the 2048 originals) because the editor's Android export caps them with `GAM300_ANDROID_MAX_TEXTURE_SIZE`, and the runtime applies the same cap when loading. To ship 2048 textures on Android, raise that cache entry (for example to 4096) for both the editor build that exports the Android assets and the Android engine build, then re-export the Android resources. Expect roughly four times the texture memory on the device.

### Profiling on device

The engine has a lightweight frame profiler that needs no Tracy connection. Configure with `-DGAM300_FRAME_STATS=ON` (for Android, set `GAM300_FRAME_STATS` to `ON` in `AndroidProject/app/src/main/cpp/CMakeLists.txt` and run clean-android.bat so the engine is reconfigured). Every five seconds the game then logs one block tagged `[FrameStats]` with the average, 95th percentile, and worst frame time for that window followed by the most expensive profiling zones (average and worst milliseconds per frame, calls per frame). On Android read it with `adb logcat -s GAM300`; on desktop it is written to stdout. The zones are the same `PROFILE_SCOPED` / `PROFILE_PLOT_TIMED` markers Tracy uses, so the numbers line up with the editor's Tracy captures. Only main-thread zones are reported. Leave the option off for shipping builds; each zone costs two clock reads.

On desktop game builds, `GAM300_START_SCENE=Scenes/04_Level.scene ./Kusane` boots straight into a scene instead of the splash screen, which keeps profiling and bug repros from having to click through the menus. On desktop, `GAM300_FRAME_STATS_INTERVAL=2` changes the report interval in seconds.

The report also carries a `Lua::HeapKB` counter (the Lua heap size, so a garbage-collection sawtooth is visible) and `GL::DrawCalls` / `GL::DepthDrawCalls` counters (main-pass and shadow-pass draw calls per frame). If the Lua heap keeps growing, `GAM300_LUA_HEAP_REPORT=20` (desktop, seconds) additionally forces two full collections every 20 seconds and prints the reachable table groups with the largest growth since the previous report plus reachable value counts by type, which is enough to name the table a script keeps appending to. It pauses the game for tens of milliseconds each time, so leave it unset for frame-time measurements.

Some Android binaries are not available, so some of them are manually compiled. A few scripts are provided to compile them currently in Project/Libraries/build-scripts. They will eventually be cleaned up for a more proper CI/CD pipeline. Currently they only need to be compiled once, so I have already compiled them, and the developers need not do anything unless they want to use another version of the libraries.

### Frequent Errors for Android Studio

On first install, you might need to clean project before it can build.

If you see any CMake error, try running clean-android.bat. Then, clean project in Android Studio. Then try running again.

If you see something like ndk="" as an error, you might need to run setup-android-ndk.bat and restart your computer.

If you see an error telling you to update NDK, do it. The default NDK isntalled when you install Android Studio is NDK 27. However, I've set Android Studio to explicitly use NDK 29, the latest version.

Lastly, make sure you have NDK and CMake installed/ticked in Files - Tools - SDK Manager - SDK Tools.

## End

Written by Yan Yu, the tech lead. Please contact if there are any issues.
