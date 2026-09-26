<p align="center">
  <a href="https://yyan115.github.io/GAM300/">
    <img src="docs/media/kusane-readme.svg" alt="Kusane" width="640">
  </a>
</p>

<p align="center">
  <strong>A 3D hack-and-slash built on a custom C++ engine.</strong><br>
  Created by Team Marbles at Singapore Institute of Technology and DigiPen Institute of Technology.
</p>

<p align="center">
  <a href="https://yyan115.github.io/GAM300/"><strong>Visit the website</strong></a> ·
  <a href="https://github.com/yyan115/GAM300/releases/latest"><strong>Download the game</strong></a> ·
  <a href="https://github.com/yyan115/GAM300/issues">Report an issue</a>
</p>

<p align="center">
  <a href="https://github.com/yyan115/GAM300/actions/workflows/build-test.yml"><img src="https://github.com/yyan115/GAM300/actions/workflows/build-test.yml/badge.svg?branch=main" alt="Build status"></a>
  <a href="https://github.com/yyan115/GAM300/releases/latest"><img src="https://img.shields.io/github/v/release/yyan115/GAM300?color=b43b2d" alt="Latest release"></a>
</p>

## About

The Karasu have destroyed Kusane’s clan and taken her wings. Armed with a temple weapon, she fights to reclaim her home.

Combine sword attacks, pull enemies into reach with the chain, slam airborne enemies to the ground, and collect feathers to power magic attacks. Explore the temple and face the Karasu in the Battle Dome.

This repository contains the game, the 3D engine itself, its desktop scene editor, Lua gameplay scripts, and the Windows, Linux and Android build pipelines. Visit the [website](https://yyan115.github.io/GAM300/) for gameplay clips, screenshots and the full team credits.

## Play

Ready-to-play builds are available from [GitHub Releases](https://github.com/yyan115/GAM300/releases/latest).

| Platform | Download | How to run |
| --- | --- | --- |
| Windows · x64 | [Installer](https://github.com/yyan115/GAM300/releases/latest/download/Kusane_Setup.exe) | Run `Kusane_Setup.exe` and follow the setup wizard. |
| Linux · x86_64 | [AppImage](https://github.com/yyan115/GAM300/releases/latest/download/Kusane-x86_64.AppImage) | Make the AppImage executable, then launch it. |
| Android · ARM64 | [APK](https://github.com/yyan115/GAM300/releases/latest/download/Kusane.apk) | Install `Kusane.apk` on an Android 13 or newer device. |

Desktop builds require OpenGL 4.5. The Linux AppImage requires glibc 2.38 or newer. Ubuntu 24.04 and Debian 13 have been validated. Android uses on-screen touch controls. Desktop controls are available from the game’s main menu.

## Build from source

The desktop engine and editor use **C++20, CMake, Ninja and vcpkg**. Android uses the **Gradle wrapper and Android NDK**. Gameplay is written in **Lua**.

```bash
git clone https://github.com/yyan115/GAM300.git
cd GAM300
```

**Run the desktop editor first to compile assets before launching a standalone game build.** Android assets must be generated using the editor’s **File → Compile Assets for Android** command.

See the [build guide](docs/BUILDING.md) for prerequisites, commands and troubleshooting.

- [Windows](docs/BUILDING.md#windows)
- [Linux](docs/BUILDING.md#linux)
- [Android](docs/BUILDING.md#android)
- [Creating a release](docs/BUILDING.md#creating-a-release)

## Repository layout

| Path | Contents |
| --- | --- |
| [`Project/Engine/`](Project/Engine/) | Rendering, ECS, physics, animation, audio, input and asset management. |
| [`Project/Game/`](Project/Game/) | Game entry point and engine integration. |
| [`Project/Editor/`](Project/Editor/) | Desktop scene editor and asset compilation tools. |
| [`Project/Scripting/`](Project/Scripting/) | Lua runtime and C++ bindings. |
| [`Project/Resources/`](Project/Resources/) | Authored scenes, models, textures, audio, shaders and gameplay scripts. |
| [`Project/Tools/`](Project/Tools/) | Asset cooking, packaging and validation utilities. |
| [`AndroidProject/`](AndroidProject/) | Android application, native integration and exported mobile assets. |
| [`Installer/`](Installer/) | Windows installer configuration and artwork. |
| [`docs/`](docs/) | Game website, media and development documentation. |
| [`.github/workflows/`](.github/workflows/) | Build checks and release packaging. |

## Libraries

- Jolt Physics
- Lua and LuaBridge
- FMOD
- Dear ImGui and ImGuizmo
- Assimp
- FreeType
- meshoptimizer
- GLFW and GLAD
- GLM and GLI
- RapidJSON and spdlog

The engine builds as a shared library. The standalone game links against it. Editor configurations also link the game as a static library so it can run inside the editor.

## Issues and contributions

Please [open an issue](https://github.com/yyan115/GAM300/issues) for bugs or proposed changes. For a bug report, include the release version, platform, device or CPU/GPU, steps to reproduce, and any relevant logs or recordings.

Keep pull requests focused and describe the behaviour changed and how it was tested. Check affected desktop and mobile paths, and regenerate Android assets through the editor when changing shared game resources. Discuss substantial engine or gameplay changes in an issue first.

## Credits and notices

Developed by **Team Marbles** as a student project at **Singapore Institute of Technology and DigiPen Institute of Technology**.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.

Third-party software, fonts and audio retain their respective licenses and attributions. See [`Project/Distribution/Licenses/`](Project/Distribution/Licenses/) for the notices included with the game.
