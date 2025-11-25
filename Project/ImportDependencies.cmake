include(FetchContent)

# Function to import Jolt Physics
function(import_jolt)
    if(NOT TARGET Jolt)  # Guard to prevent multiple inclusion
        message(STATUS "Importing Jolt Physics...")

        FetchContent_Declare(
            JoltPhysics
            GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
            GIT_TAG        v5.4.0
            SOURCE_SUBDIR  Build
        )

        # Configure Jolt Physics build options
        set(TARGET_UNIT_TESTS OFF CACHE BOOL "Build Jolt unit tests" FORCE)
        set(TARGET_HELLO_WORLD OFF CACHE BOOL "Build Jolt hello world example" FORCE)
        set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "Build Jolt performance tests" FORCE)
        set(TARGET_SAMPLES OFF CACHE BOOL "Build Jolt samples" FORCE)
        set(TARGET_VIEWER OFF CACHE BOOL "Build Jolt viewer" FORCE)
        set(DOUBLE_PRECISION OFF CACHE BOOL "Use double precision" FORCE)

        # Disable certain Jolt features for both platforms
        set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "Disable object stream" FORCE)
        set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "Disable FP exceptions" FORCE)
        set(PROFILE_ENABLED OFF CACHE BOOL "Disable profiling (Android)" FORCE)

        # Android-specific: Disable IPO which causes toolchain issues
        if(ANDROID)
            set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable IPO for Android" FORCE)
            set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "Disable cross-platform determinism for Android" FORCE)
            set(USE_STD_VECTOR ON CACHE BOOL "Use std::vector for Android" FORCE)
            set(OBJECT_LAYER_BITS 16 CACHE STRING "Use 16 bit object layers for Android" FORCE)

            # Enable RTTI for Android using Jolt's official option
            set(CPP_RTTI_ENABLED ON CACHE BOOL "Enable RTTI for Jolt on Android" FORCE)
            set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "Enable exceptions for Jolt on Android" FORCE)

            # Enable position independent code for shared library linking
            set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Enable -fPIC for Android" FORCE)
        endif()

        # Download Jolt first (but don't build yet) so we can patch it
        FetchContent_GetProperties(JoltPhysics)
        if(NOT joltphysics_POPULATED)
            FetchContent_Populate(JoltPhysics)

            # Patch Jolt's CMakeLists to export compile definitions as INTERFACE
            # This ensures Engine sees the same defines Jolt was compiled with
            file(READ "${joltphysics_SOURCE_DIR}/Build/CMakeLists.txt" JOLT_CMAKE)
            # Find the add_library(Jolt ...) and add INTERFACE definitions after it
            string(REGEX REPLACE
                "(add_library\\(Jolt[^)]+\\))"
                "\\1\n\n# Export critical defines so consumers match Jolt's build config\ntarget_compile_definitions(Jolt INTERFACE \$<TARGET_PROPERTY:Jolt,COMPILE_DEFINITIONS>)"
                JOLT_CMAKE "${JOLT_CMAKE}")
            file(WRITE "${joltphysics_SOURCE_DIR}/Build/CMakeLists.txt" "${JOLT_CMAKE}")
            message(STATUS "  Patched Jolt to export compile definitions")

            # Now build it
            add_subdirectory(${joltphysics_SOURCE_DIR}/Build ${joltphysics_BINARY_DIR})
        endif()

        # Jolt doesn't export include directories automatically, add them manually
        # This is a quirk of Jolt's CMake - it expects you to include from parent directory
        if(TARGET Jolt)  # Check if target was created
            get_target_property(JOLT_SOURCE_DIR Jolt SOURCE_DIR)
            if(JOLT_SOURCE_DIR)
                get_filename_component(JOLT_INCLUDE_DIR "${JOLT_SOURCE_DIR}" DIRECTORY)
                # Use BUILD_INTERFACE generator expression to make this build-tree only
                target_include_directories(Jolt INTERFACE
                    $<BUILD_INTERFACE:${JOLT_INCLUDE_DIR}>
                )
                message(STATUS "  Added Jolt include directory: ${JOLT_INCLUDE_DIR}")
            endif()

            # Android: Disable custom allocator for compatibility
            if(ANDROID)
                target_compile_definitions(Jolt PUBLIC JPH_DISABLE_CUSTOM_ALLOCATOR)
                message(STATUS "  Disabled custom allocator for Jolt on Android")
            endif()

            message(STATUS "Jolt Physics imported successfully.")
        else()
            message(FATAL_ERROR "Failed to create Jolt target - FetchContent may have failed")
        endif()
    else()
        message(STATUS "Jolt Physics already imported, skipping.")
    endif()
endfunction()

# Function to import Compressonator (for Editor texture compression)
function(import_compressonator)
    if(TARGET Compressonator)
        message(STATUS "Compressonator already imported, skipping.")
        return()
    endif()

    message(STATUS "Fetching Compressonator from GitHub...")

    FetchContent_Declare(
        Compressonator
        GIT_REPOSITORY https://github.com/GPUOpen-Tools/compressonator.git
        GIT_TAG        V4.5.52
        GIT_SHALLOW    FALSE
    )

    # Fetch but don't build yet - we need to patch the CMakeLists first
    FetchContent_GetProperties(Compressonator)
    if(NOT compressonator_POPULATED)
        FetchContent_Populate(Compressonator)

        message(STATUS "Patching Compressonator CMakeLists to skip external dependencies...")

        # Read the main CMakeLists.txt
        file(READ "${compressonator_SOURCE_DIR}/CMakeLists.txt" CMP_CMAKE)

        # Comment out the line that includes external dependencies
        # The actual line is: include(external/CMakeLists.txt)
        string(REPLACE
            "include(external/CMakeLists.txt)"
            "# include(external/CMakeLists.txt) # PATCHED: Skip external deps"
            CMP_CMAKE "${CMP_CMAKE}")

        # Also skip the other external cmake includes
        string(REPLACE
            "include(external/cmake/CMakeLists.txt)"
            "# include(external/cmake/CMakeLists.txt) # PATCHED"
            CMP_CMAKE "${CMP_CMAKE}")

        # Also skip ALL applications subdirectory adds (GUI, CLI, plugins)
        string(REGEX REPLACE
            "add_subdirectory\\([^)]*applications[^)]*\\)"
            "# \\0 # PATCHED"
            CMP_CMAKE "${CMP_CMAKE}")

        # Also skip external library subdirectories (imgui, glew, glm, etc.)
        string(REGEX REPLACE
            "add_subdirectory\\(external/[^)]+\\)"
            "# \\0 # PATCHED"
            CMP_CMAKE "${CMP_CMAKE}")

        # Fix: Comment out add_definitions(-std=c++14) which incorrectly applies to C files
        string(REPLACE
            "add_definitions(-std=c++14)"
            "# add_definitions(-std=c++14) # PATCHED: Don't apply C++ flags to C files"
            CMP_CMAKE "${CMP_CMAKE}")

        file(WRITE "${compressonator_SOURCE_DIR}/CMakeLists.txt" "${CMP_CMAKE}")
        message(STATUS "Compressonator CMakeLists patched")

        # Patch pluginbase.h to add missing include for uint32_t
        set(PLUGINBASE_PATH "${compressonator_SOURCE_DIR}/applications/_plugins/common/pluginbase.h")
        if(EXISTS "${PLUGINBASE_PATH}")
            file(READ "${PLUGINBASE_PATH}" PLUGINBASE_CONTENT)
            string(REPLACE
                "#ifndef _PLUGINBASE_H\n#define _PLUGINBASE_H"
                "#ifndef _PLUGINBASE_H\n#define _PLUGINBASE_H\n\n#include <cstdint>  // PATCHED: Added for uint32_t"
                PLUGINBASE_CONTENT "${PLUGINBASE_CONTENT}")
            file(WRITE "${PLUGINBASE_PATH}" "${PLUGINBASE_CONTENT}")
            message(STATUS "Patched pluginbase.h to add <cstdint> include")
        endif()

        # Patch codec_dxtc_alpha.cpp to fix duplicate function name conflict
        set(CODEC_DXTC_ALPHA_PATH "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/dxtc/codec_dxtc_alpha.cpp")
        if(EXISTS "${CODEC_DXTC_ALPHA_PATH}")
            file(READ "${CODEC_DXTC_ALPHA_PATH}" CODEC_CONTENT)
            # Rename the local function to avoid conflict with bcn_common_kernel.h version
            string(REPLACE
                "static uint64_t cmp_getBlockPackedIndicesSNorm("
                "static uint64_t cmp_getBlockPackedIndicesSNorm_local("
                CODEC_CONTENT "${CODEC_CONTENT}")
            # Update the call site to use the renamed function
            string(REPLACE
                "BC4_Snorm_block.data = cmp_getBlockPackedIndicesSNorm(alphaMinMax, alphaBlockSnorm, BC4_Snorm_block.data);"
                "BC4_Snorm_block.data = cmp_getBlockPackedIndicesSNorm_local(alphaMinMax, alphaBlockSnorm, BC4_Snorm_block.data);"
                CODEC_CONTENT "${CODEC_CONTENT}")
            file(WRITE "${CODEC_DXTC_ALPHA_PATH}" "${CODEC_CONTENT}")
            message(STATUS "Patched codec_dxtc_alpha.cpp to fix function name conflict")
        endif()

        # Now add the subdirectory with our options
        # Following the official docs: disable all apps, enable only SDK
        set(OPTION_ENABLE_ALL_APPS OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_CMP_SDK ON CACHE BOOL "" FORCE)
        set(OPTION_BUILD_APPS_CMP_CLI OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_APPS_CMP_GUI OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_GUI OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_KTX2 OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_DRACO OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_EXR OFF CACHE BOOL "" FORCE)
        set(OPTION_BUILD_ASTC OFF CACHE BOOL "" FORCE)
        set(OPTION_CMP_QT OFF CACHE BOOL "" FORCE)
        set(OPTION_CMP_OPENCV OFF CACHE BOOL "" FORCE)

        add_subdirectory(${compressonator_SOURCE_DIR} ${compressonator_BINARY_DIR})
    endif()

    # The library target should be CMP_Framework or similar - check what was created
    if(TARGET CMP_Framework)
        add_library(Compressonator ALIAS CMP_Framework)
        message(STATUS "Compressonator SDK built from source (target: CMP_Framework)")
    elseif(TARGET CompressonatorLib)
        add_library(Compressonator ALIAS CompressonatorLib)
        message(STATUS "Compressonator SDK built from source (target: CompressonatorLib)")
    else()
        message(WARNING "Compressonator build did not create expected target (CMP_Framework or CompressonatorLib)")
    endif()
endfunction()

# Function to import all dependencies
function(importDependencies)
    message(STATUS "Starting to import dependencies...")

    # Only use FetchContent for Android - Desktop uses vcpkg
    if(ANDROID)
        import_jolt()
        message(STATUS "Android: Using FetchContent for Jolt Physics")
    else()
        message(STATUS "Desktop: Jolt and other dependencies handled by vcpkg")
    endif()

    # Compressonator for Editor texture compression (Linux builds from source, Windows uses prebuilt)
    if(CMAKE_BUILD_TYPE MATCHES "Editor" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        import_compressonator()
        message(STATUS "Linux Editor: Using FetchContent for Compressonator")
    endif()

    message(STATUS "All dependencies have been imported successfully.")
endfunction()
