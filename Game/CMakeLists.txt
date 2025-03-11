#
# CMakeLists for the Game subproject
#
# In normal builds (Debug/Release), we build Game as an executable (Game.exe).
# In Editor builds (EditorDebug/EditorRelease), we build Game as a static library (Game.lib)
# so that Editor can link to it directly.
#

project(${GAME_NAME} LANGUAGES CXX)

# 1) Gather all Game source/header files
file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

# 2) Check if we're in an "Editor" build configuration
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
if(BUILD_TYPE_UPPER MATCHES "EDITORDEBUG" OR BUILD_TYPE_UPPER MATCHES "EDITORRELEASE")
    message(STATUS "Building ${GAME_NAME} as a STATIC LIB for Editor mode.")

    # Create a static library that the Editor can link to
    add_library(${GAME_NAME} STATIC ${GAME_SOURCES})
    # Link to Engine
    target_link_libraries(${GAME_NAME} PUBLIC ${ENGINE_NAME})

else()
    message(STATUS "Building ${GAME_NAME} as an EXECUTABLE for normal game mode.")

    # Create an executable with a minimal main() and link the game logic
    # We'll assume "GameCore.cpp" etc. are in the same set of sources
    add_executable(${GAME_NAME} ${GAME_SOURCES})
    target_link_libraries(${GAME_NAME} PRIVATE ${ENGINE_NAME})
endif()

# 3) Include directories for the Game code
target_include_directories(${GAME_NAME} PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
)
