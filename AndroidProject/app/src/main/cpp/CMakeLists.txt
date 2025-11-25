cmake_minimum_required(VERSION 3.22.1)
project("gam300android")

# Path to the main engine project (relative to this CMakeLists.txt)
set(ENGINE_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Project")
set(ENGINE_BUILD_DIR "${ENGINE_PROJECT_DIR}/Build/android-debug")

# Correct jniLibs path for Android Studio (arm64 only here; keep same layout as before)
set(JNILIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/arm64-v8a)

# Ensure jniLibs directory exists
file(MAKE_DIRECTORY ${JNILIB_DIR})

#
# --- Configure Engine if CMakeCache doesn't exist (one-time setup) ---
#
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

#
# --- Source/dependency tracking for Engine & Game (so rebuild triggers) ---
#
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

file(GLOB ENGINE_CMAKE_FILES
        "${ENGINE_PROJECT_DIR}/Engine/CMakeLists.txt"
        "${ENGINE_PROJECT_DIR}/Game/CMakeLists.txt"
        "${ENGINE_PROJECT_DIR}/CMakeLists.txt"
        "${ENGINE_PROJECT_DIR}/ImportDependencies.cmake"
)

#
# --- Build Engine and Game when sources change ---
#
add_custom_command(
        OUTPUT
        ${ENGINE_BUILD_DIR}/libEngine.so
        ${ENGINE_BUILD_DIR}/libGame.a
        COMMAND ${CMAKE_COMMAND} -E echo "[Android CMake] Building Engine and Game..."
        COMMAND ${CMAKE_COMMAND} --build ${ENGINE_BUILD_DIR}
        WORKING_DIRECTORY ${ENGINE_PROJECT_DIR}
        DEPENDS ${ENGINE_SOURCES} ${GAME_SOURCES_FOR_DEPS} ${ENGINE_CMAKE_FILES}
        COMMENT "Building Engine and Game (auto-rebuild when sources change)"
        VERBATIM
)

add_custom_target(build_engine_and_game
        DEPENDS
        ${ENGINE_BUILD_DIR}/libEngine.so
        ${ENGINE_BUILD_DIR}/libGame.a
)

#
# --- Detect actual locations of Engine / Game outputs (robust) ---
#
# Try several common locations and pick the first that exists.
set(_candidate_engine_paths
        "${ENGINE_BUILD_DIR}/libEngine.so"
        "${ENGINE_BUILD_DIR}/Engine/libEngine.so"
        "${ENGINE_BUILD_DIR}/lib/libEngine.so"
        "${ENGINE_BUILD_DIR}/bin/Engine/libEngine.so"
)

set(_candidate_game_paths
        "${ENGINE_BUILD_DIR}/libGame.a"
        "${ENGINE_BUILD_DIR}/Game/libGame.a"
        "${ENGINE_BUILD_DIR}/lib/libGame.a"
        "${ENGINE_BUILD_DIR}/bin/Game/libGame.a"
)

set(ENGINE_SO_PATH "")
set(GAME_A_PATH "")

foreach(p ${_candidate_engine_paths})
    if(EXISTS ${p})
        set(ENGINE_SO_PATH ${p})
        break()
    endif()
endforeach()

foreach(p ${_candidate_game_paths})
    if(EXISTS ${p})
        set(GAME_A_PATH ${p})
        break()
    endif()
endforeach()

if(NOT ENGINE_SO_PATH)
    message(WARNING "libEngine.so not found in expected locations under: ${ENGINE_BUILD_DIR}")
    message(STATUS "Checked: ${_candidate_engine_paths}")
    # Do not fatal here — the build step may produce it on first build (we depend on build_engine_and_game)
endif()

if(NOT GAME_A_PATH)
    message(WARNING "libGame.a not found in expected locations under: ${ENGINE_BUILD_DIR}")
    message(STATUS "Checked: ${_candidate_game_paths}")
endif()

# Print resolved paths for debugging
if(ENGINE_SO_PATH)
    message(STATUS "Resolved Engine .so: ${ENGINE_SO_PATH}")
endif()
if(GAME_A_PATH)
    message(STATUS "Resolved Game .a: ${GAME_A_PATH}")
endif()

#
# --- Copy Engine library with dependency tracking ---
#
# If the engine .so wasn't detected yet, still create a copy rule that depends
# on the build target. The DEPENDS field will allow the build to produce the file.
set(ENGINE_COPY_SRC "${ENGINE_SO_PATH}")
if(NOT ENGINE_COPY_SRC)
    # fallback path equal to typical expected path — keeps DEPENDS logic valid
    set(ENGINE_COPY_SRC "${ENGINE_BUILD_DIR}/libEngine.so")
endif()

add_custom_command(
        OUTPUT ${JNILIB_DIR}/libEngine.so
        COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_COPY_SRC} ${JNILIB_DIR}/
        DEPENDS ${ENGINE_COPY_SRC} build_engine_and_game
        COMMENT "Copying Engine library to jniLibs"
        VERBATIM
)

#
# --- Copy FMOD library (prebuilt) ---
#
set(FMOD_SRC "${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so")
add_custom_command(
        OUTPUT ${JNILIB_DIR}/libfmod.so
        COMMAND ${CMAKE_COMMAND} -E copy ${FMOD_SRC} ${JNILIB_DIR}/
        DEPENDS ${FMOD_SRC}
        COMMENT "Copying FMOD library to jniLibs"
        VERBATIM
)

#
# --- Copy assimp library (prebuilt) ---
#
set(ASSIMP_SRC "${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libassimp.so")
add_custom_command(
        OUTPUT ${JNILIB_DIR}/libassimp.so
        COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_SRC} ${JNILIB_DIR}/
        DEPENDS ${ASSIMP_SRC}
        COMMENT "Copying assimp library to jniLibs"
        VERBATIM
)

#
# --- Copy C++ shared library from NDK (libc++_shared) ---
#
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(NDK_HOST_TAG "windows-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(NDK_HOST_TAG "linux-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(NDK_HOST_TAG "darwin-x86_64")
else()
    message(FATAL_ERROR "Unsupported host platform: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

set(LIBCXX_SRC "${ANDROID_NDK}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so")
add_custom_command(
        OUTPUT ${JNILIB_DIR}/libc++_shared.so
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBCXX_SRC} ${JNILIB_DIR}/
        COMMENT "Copying C++ shared library from NDK to jniLibs"
        VERBATIM
)

#
# --- Aggregate copy target to ensure all libs are present ---
#
add_custom_target(copy_libraries ALL
        DEPENDS
        ${JNILIB_DIR}/libEngine.so
        ${JNILIB_DIR}/libfmod.so
        ${JNILIB_DIR}/libassimp.so
        ${JNILIB_DIR}/libc++_shared.so
)

#
# --- JNI bridge library (your JNI code) ---
#
add_library(${CMAKE_PROJECT_NAME} SHARED
        native-lib.cpp
)

# Post-build: copy the generated JNI lib to jniLibs as well
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${JNILIB_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${JNILIB_DIR}/
        COMMENT "Copying libgam300android.so to jniLibs"
        VERBATIM
)

#
# --- Include directories (preserve exactly as before) ---
#
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${ENGINE_PROJECT_DIR}/Engine/include
        ${ENGINE_PROJECT_DIR}/Game/include
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/glm
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/rapidjson
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/assimp
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/spdlog
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/freetype2
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/fmod
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/GLI
        ${ENGINE_PROJECT_DIR}/Libraries/android/headers/hfsm2
        ${ENGINE_BUILD_DIR}/_deps/joltphysics-src
)

#
# --- Import prebuilt Engine and Game for linking ---
#
# ENGINE: imported shared object will be read from jniLibs (copied earlier)
add_library(Engine SHARED IMPORTED)
set_target_properties(Engine PROPERTIES
        IMPORTED_LOCATION ${JNILIB_DIR}/libEngine.so
)

# GAME: static library location (resolve similarly to engine if needed)
set(GAME_IMPORT_LOCATION "${GAME_A_PATH}")
if(NOT GAME_IMPORT_LOCATION)
    set(GAME_IMPORT_LOCATION "${ENGINE_BUILD_DIR}/libGame.a")
endif()

add_library(Game STATIC IMPORTED)
set_target_properties(Game PROPERTIES
        IMPORTED_LOCATION ${GAME_IMPORT_LOCATION}
)

#
# --- Link JNI bridge against game/engine and libs (preserve behaviour) ---
#
# Ensure JNI depends on the Engine/Game build and library copies
add_dependencies(${CMAKE_PROJECT_NAME} build_engine_and_game copy_libraries)

target_link_libraries(${CMAKE_PROJECT_NAME}
        Game
        Engine
        ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so
        c++_shared
        android
        log
        GLESv3
        EGL
)

# End of file
