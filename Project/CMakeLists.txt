cmake_minimum_required(VERSION 3.20)

# Tell CMake that EditorDebug is a Debug-type config, EditorRelease is Release-type
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;EditorDebug;EditorRelease" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;EditorDebug")

# Map custom configurations to standard ones for vcpkg imported targets
# Without this, vcpkg doesn't know EditorRelease should use Release libraries
set(CMAKE_MAP_IMPORTED_CONFIG_EDITORRELEASE Release)
set(CMAKE_MAP_IMPORTED_CONFIG_EDITORDEBUG Debug)

# vcpkg integration - for Linux and Windows builds
# Only Android uses prebuilt libraries
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Project definition
project(GAM300 VERSION 1.0.0 LANGUAGES C CXX)

# MSVC Runtime Library - automatic Debug/Release selection
# DEBUG_CONFIGURATIONS property tells CMake which configs are "debug"
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:EditorDebug>>:Debug>DLL")

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C Standard (for C files like glad.c and Lua sources)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Import dependencies using centralized module
include(ImportDependencies.cmake)
importDependencies()

# Build configurations
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-specific compiler flags
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_EDITORDEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_EDITORRELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_EDITORDEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_EDITORRELEASE "${CMAKE_C_FLAGS_RELEASE}")
elseif(MSVC)
    # MSVC-specific flags
    # NOTE: Do NOT add /MD or /MDd here - CMAKE_MSVC_RUNTIME_LIBRARY handles it
    # Adding explicit runtime flags will override and conflict with that setting
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")

    # Debug configs: /Zi (debug info), /Od (no optimization), /RTC1 (runtime checks)
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /RTC1")
    set(CMAKE_CXX_FLAGS_EDITORDEBUG "/Zi /Od /RTC1")
    set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /RTC1")
    set(CMAKE_C_FLAGS_EDITORDEBUG "/Zi /Od /RTC1")

    # Release configs: /O2 (optimize), /DNDEBUG
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_EDITORRELEASE "/O2 /DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_C_FLAGS_EDITORRELEASE "/O2 /DNDEBUG")
endif()

# Output directories - separate clean output from intermediate files
# OUTPUT_DIR is set by presets, defaults to CMAKE_BINARY_DIR if not set
if(NOT OUTPUT_DIR)
    set(OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})  # .lib files stay in intermediate

# Platform-specific dependencies
if(ANDROID)
    # Android uses OpenGL ES, not desktop OpenGL
    find_library(GLES3_LIBRARY GLESv3)
    find_library(EGL_LIBRARY EGL)
    set(OPENGL_LIBRARIES ${GLES3_LIBRARY} ${EGL_LIBRARY})
else()
    # Desktop platforms (Linux/Windows) use OpenGL
    find_package(OpenGL REQUIRED)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
    endif()
endif()

# Add Lua
add_subdirectory("${CMAKE_SOURCE_DIR}/Libraries/lua")

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