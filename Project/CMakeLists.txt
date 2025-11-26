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
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake"
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

# MSVC warning suppressions
if(MSVC)
    add_compile_options(
        # C4251: 'class X needs dll-interface to be used by clients of class Y'
        # Source: Engine headers with exported classes containing STL members
        # Reason: Safe when all code is built together with same compiler/runtime.
        #         Only a real issue when distributing DLLs to third parties.
        /wd4251

        # C4275: 'non dll-interface class X used as base for dll-interface class Y'
        # Source: Engine headers with exported classes containing STL members
        # Reason: Safe when all code is built together with same compiler/runtime.
        #         Only a real issue when distributing DLLs to third parties.
        /wd4275

        # C4244: 'conversion from X to Y, possible loss of data'
        # Source: Lua, rapidjson, assimp, and other third-party headers
        # Reason: Ignore third-party library warnings.
        /wd4244

        # C4267: 'conversion from size_t to int, possible loss of data'
        # Source: Lua, rapidjson, assimp, and other third-party headers
        # Reason: Ignore third-party library warnings.
        /wd4267

        # C4334: 'result of 32-bit shift implicitly converted to 64 bits'
        # Source: Lua source code (bit manipulation)
        # Reason: Ignore third-party library warnings.
        /wd4334

        # C4702: 'unreachable code'
        # Source: Lua source code
        # Reason: Ignore third-party library warnings.
        /wd4702

        # C4068: 'unknown pragma'
        # Source: FileWatch (uses #pragma mark from macOS)
        # Reason: Ignore third-party library warnings.
        /wd4068

        # C4701: 'potentially uninitialized local variable'
        # Source: Lua source code
        # Reason: Ignore third-party library warnings.
        /wd4701

        # C4310: 'cast truncates constant value'
        # Source: Lua source code
        # Reason: Ignore third-party library warnings.
        /wd4310

        # C4245: 'conversion from int to unsigned, signed/unsigned mismatch'
        # Source: ImGuizmo
        # Reason: Ignore third-party library warnings.
        /wd4245

        # C4324: 'structure was padded due to alignment specifier'
        # Source: Lua source code
        # Reason: Ignore third-party library warnings.
        /wd4324
    )
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

    # Linker flags for debug symbol generation (PDB files)
    # /DEBUG:FULL generates complete PDB files for debugging
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG:FULL" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_EDITORDEBUG "/DEBUG:FULL" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG:FULL" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_EDITORDEBUG "/DEBUG:FULL" CACHE STRING "" FORCE)
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

# Only add Editor for Editor configurations
# (Single-config generators like Ninja know CMAKE_BUILD_TYPE at configure time)
if(CMAKE_BUILD_TYPE MATCHES "Editor")
    add_subdirectory(Editor)
endif()