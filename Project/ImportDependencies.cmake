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

# Function to import all dependencies
function(importDependencies)
    message(STATUS "Starting to import dependencies...")

    import_jolt()

    message(STATUS "All dependencies have been imported successfully.")
endfunction()
