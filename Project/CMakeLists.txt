cmake_minimum_required(VERSION 3.20)

# vcpkg integration - for Linux and Windows builds
# Only Android uses prebuilt libraries
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Project definition
project(GAM300 VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
elseif(WIN32)
    # Windows-specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
endif()
set(CMAKE_CXX_FLAGS_EDITORDEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_EDITORRELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) 
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

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

# Always build Engine (always shared library)
add_subdirectory(Engine)

# Always build Game (executable or static lib depending on config)
add_subdirectory(Game)

# ------------------ Mono/.NET embedding integration (NativeMonoEmbedded-App) ------------------
# Only do work if the template exists
set(NATIVE_MONO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/NativeMonoEmbedded-App")
if(EXISTS "${NATIVE_MONO_DIR}")
    message(STATUS "NativeMonoEmbedded-App found at ${NATIVE_MONO_DIR}")

    # Tools
    find_program(DOTNET_EXECUTABLE dotnet)
    find_program(PYTHON_EXECUTABLE python3)

    if(NOT DOTNET_EXECUTABLE)
        message(WARNING "dotnet not found on PATH. You can build managed assemblies separately and skip the dotnet step.")
    endif()
    if(NOT PYTHON_EXECUTABLE)
        message(WARNING "python3 not found on PATH. prepare-runtime.py will not run automatically.")
    endif()

    # Output locations for managed output and prepared runtime
    set(MANAGED_OUT_DIR "${CMAKE_BINARY_DIR}/mono_managed")
    set(MONO_PREPARED_DIR "${NATIVE_MONO_DIR}/RuntimePrepared")  # prepare-runtime.py default may vary; adjust if needed

    # Target: build the managed assembly with dotnet (publish into MANAGED_OUT_DIR)
    if(DOTNET_EXECUTABLE)
        add_custom_target(build_managed
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MANAGED_OUT_DIR}"
            COMMAND ${DOTNET_EXECUTABLE} publish
                "${NATIVE_MONO_DIR}/Assembly-Main/Assembly-Main.csproj"
                -c Release -f net8.0 -o "${MANAGED_OUT_DIR}"
            WORKING_DIRECTORY "${NATIVE_MONO_DIR}/Assembly-Main"
            COMMENT "Building managed Assembly-Main with dotnet -> ${MANAGED_OUT_DIR}"
            VERBATIM
        )
    else()
        add_custom_target(build_managed
            COMMENT "build_managed (skipped; dotnet not found)"
        )
    endif()

    # Target: prepare the mono runtime using the template script (prepare-runtime.py)
    if(PYTHON_EXECUTABLE)
        add_custom_target(prepare_mono_runtime
            COMMAND ${PYTHON_EXECUTABLE} "${NATIVE_MONO_DIR}/prepare-runtime.py" android --abis arm64-v8a armeabi-v7a x86_64
            WORKING_DIRECTORY "${NATIVE_MONO_DIR}"
            COMMENT "Running prepare-runtime.py to download/prepare Mono runtime for Android"
            VERBATIM
        )
    else()
        add_custom_target(prepare_mono_runtime
            COMMENT "prepare_mono_runtime (skipped; python3 not found)"
        )
    endif()

    # Aggregate target
    add_custom_target(mono_setup ALL
        DEPENDS build_managed prepare_mono_runtime
        COMMENT "Managed + Mono runtime setup target"
    )

    # Add the template's native shared sources into our build (so bootstrap code is available).
    # The template should contain a CMakeLists and create a target (common names: nativeapp_shared/nativeapp)
    add_subdirectory("${NATIVE_MONO_DIR}/NativeApp.Shared")

    # If the template exported a target named nativeapp_shared, link it into your Engine or Game
    if(TARGET nativeapp_shared)
        if(TARGET Engine)
            target_link_libraries(Engine PRIVATE nativeapp_shared)
            add_dependencies(Engine mono_setup)
        endif()
        if(TARGET Game)
            target_link_libraries(Game PRIVATE nativeapp_shared)
            add_dependencies(Game mono_setup)
        endif()
    else()
        message(WARNING "nativeapp_shared target not found. Check ${NATIVE_MONO_DIR}/NativeApp.Shared/CMakeLists.txt for the actual target name and link it manually.")
    endif()

    # If you have an Android Studio module layout (app/src/main/...), copy assets to app module for packaging.
    set(ANDROID_APP_ASSETS_DIR "${CMAKE_SOURCE_DIR}/app/src/main/assets")
    if(EXISTS "${ANDROID_APP_ASSETS_DIR}")
        add_custom_target(copy_mono_assets
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ANDROID_APP_ASSETS_DIR}"
            # copy managed DLLs produced by dotnet (if present)
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${MANAGED_OUT_DIR}" "${ANDROID_APP_ASSETS_DIR}/Managed"
            # copy prepared runtime if the script outputs to a known directory; adjust source dir if different
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${NATIVE_MONO_DIR}/Runtime.Framework" "${ANDROID_APP_ASSETS_DIR}/Runtime.Framework"
            COMMENT "Copying managed DLLs and Mono runtime into Android app assets (${ANDROID_APP_ASSETS_DIR})"
            VERBATIM
        )
        add_dependencies(copy_mono_assets mono_setup)

        # Ensure Game/Engine depend on the copy so packaging sees assets when building from CMake
        if(TARGET Game)
            add_dependencies(Game copy_mono_assets)
        endif()
        if(TARGET Engine)
            add_dependencies(Engine copy_mono_assets)
        endif()
    else()
        message(STATUS "Android app assets folder not detected at ${ANDROID_APP_ASSETS_DIR}; skipping automatic asset copy.")
    endif()

else()
    message(STATUS "NativeMonoEmbedded-App not found in thirdparty; skipping mono integration.")
endif()
# ------------------------------------------------------------------------------------------------

# Only build Editor for Editor configurations
if(CMAKE_BUILD_TYPE MATCHES "Editor")
    add_subdirectory(Editor)
    message(STATUS "Building Editor for ${CMAKE_BUILD_TYPE}")
else()
    message(STATUS "Skipping Editor for ${CMAKE_BUILD_TYPE}")
endif()