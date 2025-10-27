# Force CMake to reconfigure when source directory changes
file(GLOB_RECURSE SOURCE_DEPS "src/*" "include/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE_DEPS})

# Automatically find all source files recursively
file(GLOB_RECURSE ENGINE_SOURCES
    "src/*.cpp"
    "src/*.c"
)

# Remove unused/broken source files
list(REMOVE_ITEM ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/RenderSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TestScene.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS
    "include/*.h"
    "include/*.hpp"
    "src/*.h"
    "src/*.hpp"
)

message(STATUS "Building Engine as shared library for ${CMAKE_BUILD_TYPE}")

# Engine is ALWAYS a shared library (DLL on Windows, .so on Linux/Android)
add_library(Engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# C++ Standard for Engine
set_target_properties(Engine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Define ENGINE_EXPORTS when building the shared library
target_compile_definitions(Engine PRIVATE ENGINE_EXPORTS)

# Configuration-specific preprocessor definitions matching Visual Studio
target_compile_definitions(Engine PRIVATE
    $<$<CONFIG:Debug>:_DEBUG;JPH_DEBUG_RENDERER;JPH_PROFILE_ENABLED>
    $<$<CONFIG:Release>:NDEBUG;JPH_DEBUG_RENDERER>
    $<$<CONFIG:EditorDebug>:_DEBUG;EDITOR=1;JPH_DEBUG_RENDERER;JPH_PROFILE_ENABLED>
    $<$<CONFIG:EditorRelease>:NDEBUG;EDITOR;JPH_DEBUG_RENDERER>
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(Engine PRIVATE
        _CONSOLE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Set symbol visibility for shared library
set_target_properties(Engine PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    POSITION_INDEPENDENT_CODE ON
)

# Precompiled header support (matching Visual Studio)
if(MSVC)
    target_precompile_headers(Engine PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
    )
    # Exclude glad.c from PCH
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c"
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
endif()

# Include directories
target_include_directories(Engine
    PUBLIC
        # Only expose the base include directory for consumers (Game, Editor)
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Engine internals can see everything
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform-specific dependencies and linking
if(ANDROID)
    # ============================================================================
    # ANDROID BUILD - Using prebuilt libraries + FetchContent Jolt
    # ============================================================================
    message(STATUS "Android build - using prebuilt libraries")

    # Define Android library paths
    set(ANDROID_LIBS_DIR "${CMAKE_SOURCE_DIR}/Libraries/android/arm64")
    set(ANDROID_HEADERS_DIR "${CMAKE_SOURCE_DIR}/Libraries/android/headers")

    # Prebuilt static/shared libraries
    set(SPDLOG_LIBRARY "${ANDROID_LIBS_DIR}/libspdlog.a")
    set(FREETYPE_LIBRARY "${ANDROID_LIBS_DIR}/libfreetype.a")
    set(ASSIMP_LIBRARY "${ANDROID_LIBS_DIR}/libassimp.so")
    set(FMOD_LIBRARY "${ANDROID_LIBS_DIR}/libfmod.so")

    # Verify all library files exist
    foreach(LIB_FILE ${SPDLOG_LIBRARY} ${FREETYPE_LIBRARY} ${ASSIMP_LIBRARY} ${FMOD_LIBRARY})
        if(NOT EXISTS ${LIB_FILE})
            message(FATAL_ERROR "Android library not found: ${LIB_FILE}")
        endif()
    endforeach()

    message(STATUS "Found Android prebuilt libraries:")
    message(STATUS "  spdlog: ${SPDLOG_LIBRARY}")
    message(STATUS "  freetype: ${FREETYPE_LIBRARY}")
    message(STATUS "  assimp: ${ASSIMP_LIBRARY}")
    message(STATUS "  fmod: ${FMOD_LIBRARY}")

    # Android-specific headers
    target_include_directories(Engine
        PUBLIC
            ${ANDROID_HEADERS_DIR}
            ${ANDROID_HEADERS_DIR}/glm
            ${ANDROID_HEADERS_DIR}/rapidjson
            ${ANDROID_HEADERS_DIR}/assimp
            ${ANDROID_HEADERS_DIR}/spdlog
            ${ANDROID_HEADERS_DIR}/freetype2
            ${ANDROID_HEADERS_DIR}/fmod
            ${ANDROID_HEADERS_DIR}/GLI
    )

    # Android linking
    target_link_libraries(Engine
        PUBLIC
            ${OPENGL_LIBRARIES}  # GLESv3, EGL
        PRIVATE
            Jolt                 # From FetchContent (ImportDependencies.cmake)
            ${SPDLOG_LIBRARY}
            ${FREETYPE_LIBRARY}
            ${ASSIMP_LIBRARY}
            ${FMOD_LIBRARY}
            log                  # Android log library
            android              # Android NDK library
    )

else()
    # ============================================================================
    # DESKTOP BUILD (Windows/Linux) - Using vcpkg packages
    # ============================================================================
    message(STATUS "Desktop build - using vcpkg packages")

    # vcpkg targets are already found in main CMakeLists.txt
    target_link_libraries(Engine
        PUBLIC
            ${OPENGL_LIBRARIES}
        PRIVATE
            # All dependencies are PRIVATE - Game/Editor only see Engine.h
            glm::glm
            unofficial::joltphysics::Jolt
            assimp::assimp
            glfw
            Freetype::Freetype
            glad::glad
            spdlog::spdlog
            rapidjson
            gli
    )

    # FMOD - Commercial library (manual integration, not in vcpkg)
    target_include_directories(Engine PRIVATE
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/inc
    )

    if(WIN32)
        # Windows FMOD
        target_link_libraries(Engine PRIVATE
            ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/fmod_vc.lib
        )

        # Post-build: Copy FMOD DLL to output directory
        add_custom_command(TARGET Engine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/fmod.dll
                $<TARGET_FILE_DIR:Engine>
            COMMENT "Copying FMOD DLL to output directory"
        )

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Linux FMOD
        find_library(FMOD_LIBRARY
            NAMES fmod libfmod.so.14.9 libfmod.so.14 libfmod.so
            PATHS ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64
            NO_DEFAULT_PATH
        )

        if(FMOD_LIBRARY)
            target_link_libraries(Engine PRIVATE
                ${CMAKE_DL_LIBS}
                Threads::Threads
                ${FMOD_LIBRARY}
            )
            message(STATUS "Found FMOD library: ${FMOD_LIBRARY}")

            # Set rpath for finding shared libraries
            set_target_properties(Engine PROPERTIES
                INSTALL_RPATH_USE_LINK_PATH TRUE
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "\$ORIGIN:${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64"
            )

            # Copy FMOD .so to output directory
            add_custom_command(TARGET Engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/libfmod.so.14.9
                    $<TARGET_FILE_DIR:Engine>
                COMMENT "Copying FMOD .so to output directory"
            )
        else()
            message(FATAL_ERROR "FMOD library not found for Linux")
        endif()
    endif()

    # Compressonator - Editor-only texture compression library
    if(CMAKE_BUILD_TYPE MATCHES "^Editor")
        target_include_directories(Engine PRIVATE
            ${CMAKE_SOURCE_DIR}/Libraries/Compressonator/include
        )

        if(WIN32)
            target_link_libraries(Engine PRIVATE
                $<$<CONFIG:EditorDebug>:${CMAKE_SOURCE_DIR}/Libraries/Compressonator/lib/bin/x64/Compressonator_MDd.lib>
                $<$<CONFIG:EditorRelease>:${CMAKE_SOURCE_DIR}/Libraries/Compressonator/lib/bin/x64/Compressonator_MD.lib>
            )

            # Suppress PDB mismatch warnings
            target_link_options(Engine PRIVATE /ignore:4099)
        endif()
    endif()

endif()

# Print Engine configuration summary
message(STATUS "----------------------------------------")
message(STATUS "Engine Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++20")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
if(ANDROID)
    message(STATUS "  Dependencies: Prebuilt + FetchContent")
else()
    message(STATUS "  Dependencies: vcpkg")
endif()
message(STATUS "----------------------------------------")
