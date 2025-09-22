# GAM300 Android Project - JNI Bridge to Engine
# Links with prebuilt Engine library from main project
cmake_minimum_required(VERSION 3.22.1)

project("gam300android")

# Path to the main engine project (relative to this CMakeLists.txt)
set(ENGINE_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Project")
set(ENGINE_BUILD_DIR "${ENGINE_PROJECT_DIR}/Build/android-debug")

# Configure Engine project if needed (one-time setup)
if(NOT EXISTS ${ENGINE_BUILD_DIR})
    message(STATUS "Creating Engine build directory...")
    file(MAKE_DIRECTORY ${ENGINE_BUILD_DIR})
endif()

if(NOT EXISTS ${ENGINE_BUILD_DIR}/CMakeCache.txt)
    message(STATUS "Configuring Engine for Android...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --preset android-debug
        WORKING_DIRECTORY ${ENGINE_PROJECT_DIR}
        RESULT_VARIABLE ENGINE_CONFIG_RESULT
        OUTPUT_VARIABLE ENGINE_CONFIG_OUTPUT
        ERROR_VARIABLE ENGINE_CONFIG_ERROR
    )

    if(NOT ENGINE_CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "Engine configure failed: ${ENGINE_CONFIG_ERROR}")
    endif()
endif()

# Collect all Engine source files for dependency tracking
file(GLOB_RECURSE ENGINE_SOURCES
    "${ENGINE_PROJECT_DIR}/Engine/src/*.cpp"
    "${ENGINE_PROJECT_DIR}/Engine/src/*.c"
    "${ENGINE_PROJECT_DIR}/Engine/include/*.h"
    "${ENGINE_PROJECT_DIR}/Engine/include/*.hpp"
)

# Also track CMakeLists.txt files that could affect the build
file(GLOB ENGINE_CMAKE_FILES
    "${ENGINE_PROJECT_DIR}/Engine/CMakeLists.txt"
    "${ENGINE_PROJECT_DIR}/CMakeLists.txt"
    "${ENGINE_PROJECT_DIR}/CMakePresets.json"
)

# Create custom target to build Engine when sources OR main Resources folder changes
add_custom_command(
    OUTPUT ${ENGINE_BUILD_DIR}/lib/libEngine.so
    COMMAND ${CMAKE_COMMAND} -E echo "[Android CMake] Resources changed - cleaning and rebuilding Engine..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ENGINE_BUILD_DIR}
    COMMAND ${CMAKE_COMMAND} --preset android-debug
    COMMAND ${CMAKE_COMMAND} --build ${ENGINE_BUILD_DIR} --config $<CONFIG>
    WORKING_DIRECTORY ${ENGINE_PROJECT_DIR}
    DEPENDS ${ENGINE_SOURCES} ${ENGINE_CMAKE_FILES} ${RESOURCE_FILES}
    COMMENT "Building Engine library (CLEAN + REBUILD when main Resources folder changes)"
    VERBATIM
)

# Create a target that depends on the engine library
add_custom_target(build_engine
    DEPENDS ${ENGINE_BUILD_DIR}/lib/libEngine.so
)

# Create jniLibs directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a)

# Copy Engine library with proper dependency tracking
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libEngine.so
    COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_BUILD_DIR}/lib/libEngine.so ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    DEPENDS ${ENGINE_BUILD_DIR}/lib/libEngine.so
    COMMENT "Copying Engine library to jniLibs"
    VERBATIM
)

# Copy FMOD library (static dependency)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libfmod.so
    COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    DEPENDS ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libfmod.so
    COMMENT "Copying FMOD library to jniLibs"
)

# Collect all files in Resources directory for dependency tracking
file(GLOB_RECURSE RESOURCE_FILES "${ENGINE_PROJECT_DIR}/Resources/*")

# Copy Resources from main project to Android assets - ALWAYS RUNS FIRST
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/../assets/Resources/copied.stamp
    COMMAND ${CMAKE_COMMAND} -E echo "[Android CMake] PRE-BUILD: Starting Resources copy..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_SOURCE_DIR}/../assets/Resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ENGINE_PROJECT_DIR}/Resources
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/Resources
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/../assets/Resources/copied.stamp
    COMMAND ${CMAKE_COMMAND} -E echo "[Android CMake] PRE-BUILD SUCCESS: Resources copied to assets folder"
    DEPENDS ${RESOURCE_FILES}
    COMMENT "PRE-BUILD: Copying Resources to Android assets"
    VERBATIM
)

# Create custom target for resources - runs BEFORE everything else
add_custom_target(copy_resources ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../assets/Resources/copied.stamp
)

# Create alias target for Gradle to reference
add_custom_target(copyResourcesToAssets
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../assets/Resources/copied.stamp
    COMMENT "Alias target for Gradle: Copy Resources to Android assets"
)

# Create custom target to ensure libraries are copied
add_custom_target(copy_libraries ALL
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libEngine.so
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libfmod.so
)

# Collect all Game source files except main.cpp
file(GLOB_RECURSE GAME_SOURCES "${ENGINE_PROJECT_DIR}/Game/src/*.cpp")
list(REMOVE_ITEM GAME_SOURCES "${ENGINE_PROJECT_DIR}/Game/src/main.cpp")

# Create our JNI bridge library
add_library(${CMAKE_PROJECT_NAME} SHARED
    # JNI bridge code
    native-lib.cpp
    # Game source files (excluding main.cpp)
    ${GAME_SOURCES}
)

# Include engine headers + Android library headers + Game headers
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${ENGINE_PROJECT_DIR}/Engine/include
    ${ENGINE_PROJECT_DIR}/Engine/src
    ${ENGINE_PROJECT_DIR}/Game/include
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/glm
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/rapidjson
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/assimp
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/spdlog
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/freetype2
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/fmod
    ${ENGINE_PROJECT_DIR}/Libraries/android/headers/GLI
)

# Import prebuilt libraries
add_library(Engine SHARED IMPORTED)
set_target_properties(Engine PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libEngine.so
)

add_library(assimp SHARED IMPORTED)
set_target_properties(assimp PROPERTIES
    IMPORTED_LOCATION ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libassimp.so
)

add_library(fmod SHARED IMPORTED)
set_target_properties(fmod PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libfmod.so
)

# Make sure our main library depends on the copied libraries, resources, and engine build
add_dependencies(${CMAKE_PROJECT_NAME} build_engine copy_libraries copy_resources)

# Also add the Engine sources as dependencies to our main target to force rebuilds
set_source_files_properties(native-lib.cpp PROPERTIES OBJECT_DEPENDS "${ENGINE_SOURCES}")

# Make the imported Engine library depend on the actual file being built AND copied
add_dependencies(Engine build_engine copy_libraries)

# Link everything together
target_link_libraries(${CMAKE_PROJECT_NAME}
    # Our engine
    Engine
    # Prebuilt libraries
    assimp
    fmod
    # Android system libraries
    android
    log
    # OpenGL ES for Android
    GLESv3
    EGL
)