#
# CMakeLists for the Editor subproject
#
# In EditorDebug/EditorRelease mode, Editor links to Game (which is a .lib in that mode).
# Otherwise, you can build Editor alone, but it won't have the game logic if Game is an .exe.
#

project(${EDITOR_NAME} LANGUAGES CXX)

# 1) Gather Editor source/header files
file(GLOB_RECURSE EDITOR_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

# 2) Create the Editor executable
add_executable(${EDITOR_NAME} ${EDITOR_SOURCES})

# 3) Always link Editor to the Engine
target_link_libraries(${EDITOR_NAME} PRIVATE ${ENGINE_NAME})
target_include_directories(${EDITOR_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")

# 4) If we are in an Editor build config, link Editor to the Game library
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
if(BUILD_TYPE_UPPER MATCHES "EDITORDEBUG" OR BUILD_TYPE_UPPER MATCHES "EDITORRELEASE")
    message(STATUS "Linking Editor to Game (static lib) in Editor mode.")
    target_link_libraries(${EDITOR_NAME} PRIVATE ${GAME_NAME})
endif()
