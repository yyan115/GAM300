cmake_minimum_required(VERSION 3.20)

# vcpkg integration - must be set before project()
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# Project definition
project(GAM300 VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configurations
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Linux compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_EDITORDEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_EDITORRELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) 
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages for Linux
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

# Always build Engine (always shared library)
add_subdirectory(Engine)

# Always build Game (executable or static lib depending on config)
add_subdirectory(Game)

# Only build Editor for Editor configurations
if(CMAKE_BUILD_TYPE MATCHES "Editor")
    add_subdirectory(Editor)
    message(STATUS "Building Editor for ${CMAKE_BUILD_TYPE}")
else()
    message(STATUS "Skipping Editor for ${CMAKE_BUILD_TYPE}")
endif()