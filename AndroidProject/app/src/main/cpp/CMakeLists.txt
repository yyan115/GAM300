# GAM300 Android Project - JNI Bridge to Engine
# Links with prebuilt Engine library from main project
cmake_minimum_required(VERSION 3.22.1)

project("gam300android")

# Path to the main engine project (relative to this CMakeLists.txt)
set(ENGINE_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Project")
set(ENGINE_BUILD_DIR "${ENGINE_PROJECT_DIR}/Build/android-debug")

# Configure Engine if CMakeCache doesn't exist (one-time setup)
if(NOT EXISTS ${ENGINE_BUILD_DIR}/CMakeCache.txt)
    message(STATUS "Configuring Engine for Android (one-time)...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -S ${ENGINE_PROJECT_DIR}
            -B ${ENGINE_BUILD_DIR}
            -G Ninja
            -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake
            -DANDROID_ABI=arm64-v8a
            -DANDROID_PLATFORM=android-33
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_ANDROID_STL_TYPE=c++_shared
        RESULT_VARIABLE CONFIG_RESULT
        OUTPUT_VARIABLE CONFIG_OUTPUT
        ERROR_VARIABLE CONFIG_ERROR
    )
    if(CONFIG_OUTPUT)
        message(STATUS "Configure output: ${CONFIG_OUTPUT}")
    endif()
    if(CONFIG_ERROR)
        message(STATUS "Configure errors: ${CONFIG_ERROR}")
    endif()
    if(NOT CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "Engine configure failed with code ${CONFIG_RESULT}")
    endif()
endif()

# Collect all Engine and Game source files for dependency tracking
file(GLOB_RECURSE ENGINE_SOURCES
    "${ENGINE_PROJECT_DIR}/Engine/src/*.cpp"
    "${ENGINE_PROJECT_DIR}/Engine/src/*.c"
    "${ENGINE_PROJECT_DIR}/Engine/include/*.h"
    "${ENGINE_PROJECT_DIR}/Engine/include/*.hpp"
)

file(GLOB_RECURSE GAME_SOURCES_FOR_DEPS
    "${ENGINE_PROJECT_DIR}/Game/src/*.cpp"
    "${ENGINE_PROJECT_DIR}/Game/src/*.c"
    "${ENGINE_PROJECT_DIR}/Game/include/*.h"
    "${ENGINE_PROJECT_DIR}/Game/include/*.hpp"
)

# Track CMakeLists.txt and ImportDependencies.cmake that could affect the build
file(GLOB ENGINE_CMAKE_FILES
    "${ENGINE_PROJECT_DIR}/Engine/CMakeLists.txt"
    "${ENGINE_PROJECT_DIR}/Game/CMakeLists.txt"
    "${ENGINE_PROJECT_DIR}/CMakeLists.txt"
    "${ENGINE_PROJECT_DIR}/ImportDependencies.cmake"
)

# Build Engine and Game when source files change (runs at build time, not config time)
add_custom_command(
    OUTPUT
        ${ENGINE_BUILD_DIR}/lib/libEngine.so
        ${ENGINE_BUILD_DIR}/lib/libGame.a
    COMMAND ${CMAKE_COMMAND} -E echo "[Android CMake] Building Engine and Game..."
    COMMAND ${CMAKE_COMMAND} --build ${ENGINE_BUILD_DIR}
    WORKING_DIRECTORY ${ENGINE_PROJECT_DIR}
    DEPENDS ${ENGINE_SOURCES} ${GAME_SOURCES_FOR_DEPS} ${ENGINE_CMAKE_FILES}
    COMMENT "Building Engine and Game (auto-rebuild when sources change)"
    VERBATIM
)

# Create a target that depends on the engine and game libraries
add_custom_target(build_engine_and_game
    DEPENDS
        ${ENGINE_BUILD_DIR}/lib/libEngine.so
        ${ENGINE_BUILD_DIR}/lib/libGame.a
)

# Create jniLibs directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a)

# Copy Engine library with proper dependency tracking (runs at build time)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libEngine.so
    COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_BUILD_DIR}/lib/libEngine.so ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    DEPENDS ${ENGINE_BUILD_DIR}/lib/libEngine.so
    COMMENT "Copying Engine library to jniLibs"
    VERBATIM
)

# Copy FMOD library (static dependency, only copy once)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libfmod.so
    COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    DEPENDS ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so
    COMMENT "Copying FMOD library to jniLibs"
    VERBATIM
)

# Copy assimp library (Engine dependency)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libassimp.so
    COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libassimp.so ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    DEPENDS ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libassimp.so
    COMMENT "Copying assimp library to jniLibs"
    VERBATIM
)

# Detect host platform for NDK path (Windows vs Linux)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(NDK_HOST_TAG "windows-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(NDK_HOST_TAG "linux-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(NDK_HOST_TAG "darwin-x86_64")
else()
    message(FATAL_ERROR "Unsupported host platform: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

# Copy C++ shared library from NDK (required by libgam300android.so)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libc++_shared.so
    COMMAND ${CMAKE_COMMAND} -E copy ${ANDROID_NDK}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    COMMENT "Copying C++ shared library from NDK to jniLibs"
    VERBATIM
)

# Create custom target to ensure libraries are copied
add_custom_target(copy_libraries ALL
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libEngine.so
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libfmod.so
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libassimp.so
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libc++_shared.so
)

# Create our JNI bridge library (just the bridge, not Game sources)
add_library(${CMAKE_PROJECT_NAME} SHARED
    # JNI bridge code only
    native-lib.cpp
)

# Copy the main JNI library to jniLibs after build (must come AFTER add_library)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    COMMENT "Copying libgam300android.so to jniLibs"
    VERBATIM
)

# Include only public Engine and Game headers (clean architecture!)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${ENGINE_PROJECT_DIR}/Engine/include
    ${ENGINE_PROJECT_DIR}/Game/include
    # Android headers - dependencies leaked by Engine public headers
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/glm
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/rapidjson
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/assimp
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/spdlog
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/freetype2
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/fmod
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/GLI
)

# Import the prebuilt Engine library (built by main CMake)
add_library(Engine SHARED IMPORTED)
set_target_properties(Engine PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libEngine.so
)

# Import the prebuilt Game library (built by main CMake)
add_library(Game STATIC IMPORTED)
set_target_properties(Game PROPERTIES
    IMPORTED_LOCATION ${ENGINE_BUILD_DIR}/lib/libGame.a
)

# Make sure our JNI library depends on Engine/Game being built and libraries being copied
add_dependencies(${CMAKE_PROJECT_NAME} build_engine_and_game copy_libraries)

# Link everything together
target_link_libraries(${CMAKE_PROJECT_NAME}
    # Our libraries (built by main CMake)
    Game    # Game static library
    Engine  # Engine shared library (Engine already links to assimp, fmod, Jolt, etc.)
    # FMOD for Android (needed for JNI initialization)
    ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so
    # C++ standard library (critical for Android)
    c++_shared
    # Android system libraries
    android
    log
    # OpenGL ES for Android
    GLESv3
    EGL
)