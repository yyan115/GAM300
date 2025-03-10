```
GameProject/
├── CMakeLists.txt         # Root CMake file
├── libraries/             # (Optional) Third-party dependencies (if needed)
├── build/                 # Out-of-source builds (generated)
├── Engine/                # Custom game engine
│   ├── src/
│   ├── include/
│   └── CMakeLists.txt
├── Game/                  # Actual game that uses the engine
│   ├── src/
│   ├── include/
│   └── CMakeLists.txt
├── Editor/                # IMGUI-based editor (optional)
│   ├── src/
│   ├── include/
│   └── CMakeLists.txt
├── Resources/             # Game assets (shaders, textures, sounds)
└── GAM300.sln             # (Generated) Visual Studio solution file
```
