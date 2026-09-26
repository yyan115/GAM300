# Building Kusane

[← Back to the README](../README.md)

## Before you start

Clone the repository and install Git, CMake 3.20 or newer, Ninja and a C++20 compiler. Desktop dependencies use the vcpkg revision pinned by the setup scripts. Some libraries, including FMOD and Android binaries, are provided under `Project/Libraries/`.

Run CMake preset commands from `Project/`. Run desktop executables from their build output directory so relative resource paths resolve correctly.

The first configure downloads and builds dependencies. Allow it to finish before building the game or editor.

## Windows

Install **Visual Studio 2022** with **Desktop development with C++**, including its CMake tools and Windows SDK. Ninja must be available to the selected build environment.

From the repository root:

```bat
setup-vcpkg.bat
```

Open the `Project` folder in Visual Studio using **Open a local folder**, then select `editor-release` or `editor-debug`. Build and run `Editor.exe` first, and allow asset compilation to finish. Select `release` or `debug` to build the standalone game.

The equivalent commands in a Visual Studio developer command prompt are:

```bat
cd Project
cmake --preset editor-release
cmake --build --preset editor-release --parallel
cd Build\EditorRelease
Editor.exe
```

After the editor finishes compiling assets, close it and return to `Project`:

```bat
cd ..\..
cmake --preset release
cmake --build --preset release --parallel
cd Build\Release
Kusane.exe
```

Presets are defined in [`Project/CMakePresets.json`](../Project/CMakePresets.json). VS Code with CMake Tools can use the same presets when run in an environment with MSVC configured.

## Linux

Fedora is the validated local development environment. CI also builds the Linux release on Ubuntu. Other distributions need equivalent development packages.

Install prerequisites on Fedora:

```bash
sudo dnf install -y git git-lfs cmake ninja-build gcc-c++ make zip unzip tar \
  pkgconf-pkg-config libX11-devel libXcursor-devel libXinerama-devel \
  libXrandr-devel libXi-devel libXext-devel libXrender-devel libXfixes-devel \
  libXxf86vm-devel mesa-libGLU-devel
```

From the repository root, set up dependencies and build the editor:

```bash
./setup-vcpkg.sh
cd Project
cmake --preset linux-editor-release
cmake --build --preset linux-editor-release --parallel
cd Build/LinuxEditorRelease
./Editor
```

Let the editor finish compiling assets, then close it. Build and run the game:

```bash
cd ../..
cmake --preset linux-release
cmake --build --preset linux-release --parallel
cd Build/LinuxRelease
./Kusane
```

The `linux-editor-debug` and `linux-debug` presets are also available. Linux presets live in the tracked [`Project/CMakeUserPresets.json`](../Project/CMakeUserPresets.json).

## Android

Install Android Studio and the SDK tools. The current project configuration uses:

| Component | Version / setting |
| --- | --- |
| Java runtime for Gradle | JDK 17 (used by CI) |
| Android SDK | API 36 |
| Minimum Android version | Android 13 / API 33 |
| NDK | `29.0.14033849` |
| ABI | `arm64-v8a` |
| Native build tools | CMake and Ninja |

Install the required SDK, NDK and CMake components through Android Studio’s **SDK Manager → SDK Tools**. On Windows, [`setup-android-dev.bat`](../setup-android-dev.bat) helps install the development tools. Android Studio and Gradle build the native engine directly; VS Code is optional.

### Export game assets

The checked-in mobile assets live in `AndroidProject/app/src/main/assets/Resources/`. After changing shared resources:

1. Build and open the desktop editor.
2. Select **File → Compile Assets for Android**.
3. Wait for the compilation to finish and check its output before building the APK.

For a complete clean export, remove the generated Android `Resources/` directory and `asset_manifest.txt` in the same assets directory before running the editor command. The editor regenerates the mobile formats and manifest. **Do not copy desktop resources into the Android resources directory.**

### Build and run

Open `AndroidProject/` in Android Studio, let Gradle sync, connect a device with USB debugging enabled, and run the `app` configuration.

For a release APK from the command line:

```bash
cd AndroidProject
./gradlew assembleRelease
```

Use `gradlew.bat assembleRelease` on Windows. The output is `AndroidProject/app/build/outputs/apk/release/app-release.apk`.

To view engine messages and the Android FPS log:

```bash
adb logcat -s GAM300
```

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Missing models or textures in a desktop build | Run the editor to finish asset compilation, then rebuild the game to stage the compiled resources. |
| Android shows older content | Re-export through **Compile Assets for Android**, rebuild the APK and reinstall it. |
| Android NDK or CMake configuration fails | Install the pinned NDK and SDK components, sync Gradle, and check the configured SDK path. On Windows, `clean-android.bat` can clear stale native build output. |
| Linux setup reports missing X11 or GLU packages | Install the development packages listed above; the setup script checks them before configuring vcpkg. |
| Linux AppImage reports a glibc version error | Use a distribution with glibc 2.38 or newer, or build from source on your target system. |

Desktop standalone builds start fullscreen. Add `--windowed` to launch in a window without changing the saved fullscreen preference. Set `GAM300_SHOW_FPS=1` to show the desktop frame rate in the window title during testing.

## Creating a release

The [Release Build workflow](https://github.com/yyan115/GAM300/actions/workflows/release.yml) is **manually triggered**. Pushing `main` runs build checks; pushing a version tag alone does not start installer packaging.

1. Ensure the desired game changes and editor-generated Android assets are committed and pushed.
2. Open **Actions → Release Build → Run workflow** and select the branch to release.
3. Enter a version such as `v1.1.2`, a release name such as `Kusane v1.1.2`, and release notes. Leave **Build everything but do not create a release** unchecked to publish.
4. Wait for the Windows, Linux and Android jobs to pass. The final job creates the GitHub release and uploads all three packages.

The workflow creates the version tag when publishing if it does not already exist. If you created the tag first, use that exact ref with the GitHub CLI so the built commit matches the tag:

```bash
gh workflow run release.yml --ref v1.1.2 \
  -f version=v1.1.2 \
  -f release_name="Kusane v1.1.2" \
  -f changelog="Describe the changes included in this release." \
  -f skip_publish=false
```

Enable **Build everything but do not create a release** when validating packages on a branch without publishing a release.
