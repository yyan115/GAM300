cmake_minimum_required(VERSION 3.20)

# vcpkg integration - ONLY for desktop (Windows/Linux), NOT Android
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    else()
        message(WARNING "vcpkg not found. Install vcpkg or set VCPKG_ROOT environment variable.")
    endif()
endif()

# Project definition
project(GAM300 VERSION 1.0.0 LANGUAGES CXX C)

# C++ Standard - Engine and Editor use C++20, Game uses C++17
# Individual targets will override as needed
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configurations matching Visual Studio
# Debug, Release, EditorDebug, EditorRelease
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;EditorDebug;EditorRelease" CACHE STRING "Available build configurations" FORCE)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "EditorDebug" "EditorRelease")
endif()

# Output directories matching Visual Studio's Build/ structure
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Build/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Build/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Build/Intermediate/$<CONFIG>)

# Per-configuration output directories
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_SOURCE_DIR}/Build/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_SOURCE_DIR}/Build/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_SOURCE_DIR}/Build/Intermediate/${OUTPUTCONFIG})
endforeach()

# Platform-specific compiler flags matching Visual Studio .vcxproj
if(MSVC)
    # Windows-specific flags
    add_compile_options(
        /W4                         # Warning level 4
        /EHsc                       # Exception handling model
        /MP                         # Multi-processor compilation
        /permissive-                # Standards conformance mode
    )

    # Configuration-specific flags
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /MDd" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MD /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_EDITORDEBUG "/Zi /Od /MDd" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_EDITORRELEASE "/O2 /MD /DNDEBUG" CACHE STRING "" FORCE)

    # Linker flags
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_EDITORDEBUG "/DEBUG" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_EDITORRELEASE "/DEBUG /INCREMENTAL:NO" CACHE STRING "" FORCE)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux-specific flags
    add_compile_options(
        -Wall
        -Wextra
    )
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_EDITORDEBUG "-g -O0" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_EDITORRELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)
endif()

# Platform-specific dependencies
if(ANDROID)
    # Android uses OpenGL ES
    find_library(GLES3_LIBRARY GLESv3)
    find_library(EGL_LIBRARY EGL)
    set(OPENGL_LIBRARIES ${GLES3_LIBRARY} ${EGL_LIBRARY})

    # Android uses FetchContent for Jolt Physics (different from desktop)
    include(ImportDependencies.cmake)
    importDependencies()
else()
    # Desktop platforms use OpenGL + vcpkg dependencies
    find_package(OpenGL REQUIRED)

    # Find all vcpkg packages
    find_package(assimp CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(glad CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    find_package(gli CONFIG REQUIRED)

    # Jolt Physics from vcpkg (not FetchContent on desktop!)
    find_package(unofficial-joltphysics CONFIG REQUIRED)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        set(THREADS_PREFER_PTHREAD_FLAG TRUE)
        find_package(Threads REQUIRED)
    endif()
endif()

# Always build Engine (always shared library)
add_subdirectory(Engine)

# Always build Game (executable or static lib depending on config)
add_subdirectory(Game)

# Only build Editor for Editor configurations
if(CMAKE_BUILD_TYPE MATCHES "^Editor")
    add_subdirectory(Editor)
    message(STATUS "Building Editor for ${CMAKE_BUILD_TYPE}")
else()
    message(STATUS "Skipping Editor for ${CMAKE_BUILD_TYPE}")
endif()

# Print configuration summary
message(STATUS "========================================")
message(STATUS "GAM300 Engine Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
if(NOT ANDROID)
    message(STATUS "vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
