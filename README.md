# GAM300

GAM300 - Team Marbles - C

Custom 3D game engine writtein in C++. Supports cross platform for Windows, Linux, and android. MacOS TBD.

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

Visual Studio Code IS SLOW TO UPDATE, MAY BREAK, AND IS NOT RECOMMENDED. Please use Visual Studio instead. However, Visual Studio Code might still have to be installed, for Android Studio to correctly pick up the cmake configuration. So please install Visual Studio Code as well.

Everyone should set up Android Studio to make sure their code works on Android.

Some additional library set ups may be required but not covered as everyone should already be familiar with them (E.G. GLFW, git). If possible, I will write a more comprehensive guide next time, as I currently do not know which exact libraries I need to point out.

### Visual Studios

Install Visual Studio and open the .sln file.

Select the compile option (Debug, Release, EditorDebug, EditorRelease) and select default project to run. Do note that the compile options are project dependent, I.E. Trying to compile and run the game using EditorDebug or EditorRelease will not work, and vice versa trying to compile and run the editor using Debug or Release will not work.

Do note that Visual Studios DOES NOT USE CMake.

Visual Studio does not use vcpkg. It uses precompiled binaries downloaded from the internet.

### Visual Studio Code

Visual Studio Code uses CMake and Ninja to compile and run.

Install Visual Studio Code and install official extensions for C++ and Cmake.

Conveniently, "setup-android-dev.bat" also helps to install tools for visual studio code. So please run that. More specifically, it installs cmake and ninja for you, if you'd like to install that yourself.

Lastly, run setup-vcpkg.bat to install and setup vcpkg.

You can then choose the compile option in Visual Studio Code to compile and run.

Visual Studio Code primarily uses vcpkg to source libraries. However, some libraries are not available inside, such as FMOD, so they have to be downloaded instead.

## Android Development

First install Android Studio. Go through the initial setup.

Then, run "setup-android-dev.bat" to install required tools for Android.

Once that is done, ideally you should restart your computer for best results. Then, open Android Studio and press run. If you get any errors, see section for frequent errors for Android Studio.

Android depends on Visual Studio code to compile, which is why you should have Visual Studio Code installed. You do not need Visual Studio Code open to run for Android, as I've configured the gradle and cmake so that you only need to open Android Studio to run for Android.

Some Android binaries are not available, so some of them are manually compiled. A few scripts are provided to compile them currently in Project/Libraries/build-scripts. They will eventually be cleaned up for a more proper CI/CD pipeline. Currently they only need to be compiled once, so I have already compiled them, and the developers need not do anything unless they want to use another version of the libraries.

### Frequent Errors for Android Studio

If you see any CMake error, try running -clean-cmake.bat. Then, clean project in Android Studio. Then try running again.

If you see something like ndk="" as an error, you might need to run setup-android-ndk.bat and restart your computer.

If you see an error telling you to update NDK, do it. The default NDK isntalled when you install Android Studio is NDK 27. However, I've set Android Studio to explicitly use NDK 29, the latest version.

Lastly, make sure you have NDK and CMake installed/ticked in Files - Tools - SDK Manager - SDK Tools.

## End

Written by Yan Yu, the tech lead. Please contact if there are any issues.
