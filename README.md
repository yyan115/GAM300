# GAM300

GAM300 - Team Marbles - C

Custom 3D game engine writtein in C++. Supports cross platform for Windows, Linux, and android. MacOS TBD.

## Project Structure

The project supports using either Visual Studio or Visual Studio Code. Android Studio is required for Android development.

The project is separated into 3 smaller project - Engine, Game, and Edtior.

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
- Filewatch
- FMOD
- Freetype
- GLFW
- GLAD
- GLI
- GLM
- IconFontCppHeaders
- IMGUI
- Rapidjson
- Spdlog

## Set up Development Environment

### Visual Studios

The project now supports using Visual Studio Code, but as the project was first configured using Visual Studio, it still currently supports using Visual Studio as well.

Simply install Visual Studio and open the .sln file.


TO BE FULLY UPDATED TO HAVE COMPREHENSIVE EXPLANATION OF PROJECT SETUP FOR WINDOWS, LINUX AND ANDROID.

# CMake Notes

FMOD - downloaded manually for both windows,linux, and android. for linux, copy so over to run folder
GLI - Uses vcpkg for linux, windows not working
...

# Android

filesystems becareful
use android studio and visual studio i guess for running
resources folder gets copied over to assets
use clean-android if resources folder gets changed
