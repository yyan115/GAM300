cmake_minimum_required(VERSION 3.20)

# ------------------------------------------------------------------------------
# 1) Set the main project name and language
# ------------------------------------------------------------------------------
# Use a variable so you can rename easily if desired:
set(PROJECT_NAME "GAM300")
project(${PROJECT_NAME} LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 2) Define sub-project (module) names as variables
# ------------------------------------------------------------------------------
set(ENGINE_NAME  "Engine")
set(GAME_NAME    "Game")
set(EDITOR_NAME  "Editor")

# ------------------------------------------------------------------------------
# 3) Global compiler settings
# ------------------------------------------------------------------------------
# C++17 or whichever standard you need
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# 4) Set global output directories
# ------------------------------------------------------------------------------
# This ensures that Visual Studio, CLion, or any other generator
# will place compiled libs and executables in the build/bin or build/lib folder.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Optional: helps group your projects in Visual Studio's Solution Explorer
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ------------------------------------------------------------------------------
# 5) Add subdirectories for each project
# ------------------------------------------------------------------------------
add_subdirectory(${ENGINE_NAME})
add_subdirectory(${GAME_NAME})
add_subdirectory(${EDITOR_NAME})

# ------------------------------------------------------------------------------
# 6) (Optional) Additional libraries or utility subdirectories
# e.g., add_subdirectory(External) ...
# ------------------------------------------------------------------------------

# That's it for the root CMakeLists.
# Build instructions:
#   mkdir build
#   cd build
#   cmake -G "Visual Studio 17 2022" ..
#   [ or cmake -G "Unix Makefiles" .. on Linux/Mac ]
#   [ then open GAM300.sln in build, or make -j, etc. ]
