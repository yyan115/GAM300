cmake_minimum_required(VERSION 3.15)  # Use a modern CMake version
project(GAM300)                       # Overall project name

# ------------------------------------------------------------------------------
# Centralized Variables for Project Names
# ------------------------------------------------------------------------------
set(ENGINE_PROJECT_NAME "engine")
set(GAME_PROJECT_NAME "game")
set(EDITOR_PROJECT_NAME "editor")

# ------------------------------------------------------------------------------
# Global C++ Settings
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# Option: Build Engine as a Shared Library (for dynamic linking)
# ------------------------------------------------------------------------------
option(BUILD_SHARED_ENGINE "Build engine as shared library (DLL)" ON)

# ------------------------------------------------------------------------------
# External Dependencies
# ------------------------------------------------------------------------------
# Attempt to find the libraries. These must be installed or provided in libraries/.
find_package(GLM REQUIRED)
find_package(GLFW3 REQUIRED)
find_package(GLEW REQUIRED)

# If a package isn’t found and you have the source in libraries/, you could add:
# if (NOT TARGET glfw)
#   add_subdirectory(${CMAKE_SOURCE_DIR}/libraries/glfw)
# endif()

# ------------------------------------------------------------------------------
# Add Subdirectories for Each Project
# ------------------------------------------------------------------------------
add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)

# ------------------------------------------------------------------------------
# Custom Target: Copy Resources Folder
# ------------------------------------------------------------------------------
# This target will copy the entire Resources folder into the build directory.
# This ensures that the executables have access to all necessary assets.
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Resources
            ${CMAKE_BINARY_DIR}/Resources
    COMMENT "Copying Resources to build directory"
)
