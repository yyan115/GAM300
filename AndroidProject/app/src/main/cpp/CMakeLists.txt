# GAM300 Android Project - JNI Bridge to Engine
# Links with prebuilt Engine library from main project
cmake_minimum_required(VERSION 3.22.1)

project("gam300android")

# Path to the main engine project (relative to this CMakeLists.txt)
set(ENGINE_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Project")
set(ENGINE_BUILD_DIR "${ENGINE_PROJECT_DIR}/Build/android-debug")

# Import prebuilt Engine library
add_library(Engine SHARED IMPORTED)
set_target_properties(Engine PROPERTIES
    IMPORTED_LOCATION ${ENGINE_BUILD_DIR}/lib/libEngine.so
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
)

# Import prebuilt Assimp library
add_library(assimp SHARED IMPORTED)
set_target_properties(assimp PROPERTIES
    IMPORTED_LOCATION ${ENGINE_PROJECT_DIR}/Libraries/android/arm64/libassimp.so
)

# Link everything together
target_link_libraries(${CMAKE_PROJECT_NAME}
    # Our prebuilt engine
    Engine
    # Assimp library  
    assimp
    # Android system libraries
    android
    log
    # OpenGL ES for Android
    GLESv3
    EGL
)