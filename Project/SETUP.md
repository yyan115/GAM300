# GAM300 Engine - Development Setup

## Prerequisites

- **Linux**: Any modern distribution (tested on Fedora)
- **Build Tools**: GCC/Clang, CMake 3.20+, Ninja
- **VS Code**: With recommended extensions (auto-suggested when opening project)

## Quick Start

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd Project
   ```
   
   **Note**: Dependencies are automatically installed by CMake via `vcpkg.json` manifest

2. **Open in VS Code**:
   ```bash
   code .
   ```
   - Install recommended extensions when prompted
   - VS Code will auto-configure CMake integration

3. **Build configurations available**:
   - **Debug/Release**: Game executable only
   - **EditorDebug/EditorRelease**: Editor executable (includes game as library)

## Build Methods

### Option A: VS Code (Recommended)
- Press `Ctrl+Shift+P` → "CMake: Select Configure Preset"
- Choose: Debug, Release, Editor Debug, or Editor Release  
- Press `F7` to build or click build button in status bar
- Press `F5` to debug

### Option B: Command Line
```bash
# Game Debug build
cmake --preset debug
cmake --build Build/debug

# Editor Debug build  
cmake --preset editor-debug
cmake --build Build/editor-debug
```

## Project Structure

- **Engine/**: Core engine (always built as shared library)
- **Game/**: Game code (executable OR static library depending on config)
- **Editor/**: Level editor (executable, only in Editor configs)
- **vcpkg/**: Package manager for C++ dependencies
- **Libraries/**: Manual libraries (ImGui docking, FMOD)

## Dependencies

### Managed by vcpkg:
- **Assimp**: 3D model loading
- **GLFW**: Window/input management  
- **Freetype**: Font rendering
- **GLAD**: OpenGL loader
- **GLM**: Math library
- **spdlog**: Logging
- **RapidJSON**: JSON parsing

### Manual libraries:
- **ImGui**: UI framework (docking branch for editor)
- **FMOD**: Audio engine (commercial license)

## Troubleshooting

### Build fails with missing dependencies
- Dependencies are automatically installed by CMake
- Try cleaning and rebuilding: `rm -rf Build/` then rebuild
- vcpkg uses `vcpkg.json` manifest for dependency management

### VS Code CMake not working
1. `Ctrl+Shift+P` → "CMake: Reset"
2. `Ctrl+Shift+P` → "CMake: Configure"
3. Check that vcpkg toolchain is set in settings

### Cross-platform notes
- **Windows**: Use `--triplet=x64-windows`
- **macOS**: Use `--triplet=x64-osx`
- This setup is primarily tested on Linux

## Contributing

1. Always test both Game and Editor builds before committing
2. Update this document when adding new dependencies
3. Keep vcpkg.json updated with dependency changes
4. Follow existing code style and CMake patterns