```
GAM300/
│── CMakeLists.txt               # Root-level CMake
│── build/                       # Out-of-source build directory (generated)
│── Engine/
│   ├── src/
│   ├── include/
│   ├── libraries/
│   │    ├── glm/                # Example: GLM headers only
│   │    ├── glew/               # Example: GLEW includes + maybe .lib/.dll
│   │    └── glfw/               # Example: GLFW includes + maybe .lib/.dll
│   └── CMakeLists.txt
│── Game/
│   ├── src/
│   ├── include/
│   └── CMakeLists.txt
│── Editor/
│   ├── src/
│   ├── include/
│   └── CMakeLists.txt
│── Resources/
└── GAM300.sln             # (Generated) Visual Studio solution file
```

CMake uses a configuration file called CMakeLists.txt.

Either run in Visual Studio like normally, or if you plan on using CLI:
1. Define your project in CMakeLists.txt
2. Run CMake to create the Makefile
3. Build your project using Make
4. Add code, fix things, etc then jump to step 3.
5. If you add new .c files or alter the dependencies then jump to step 1.
  
CMake tutorial: https://youtu.be/NGPo7mz1oa4?si=rCA5V6-JZyeS0jPF
