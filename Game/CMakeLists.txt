#
# CMakeLists for the Game subproject
#
# We'll create:
# 1) A STATIC library target named ${GAME_NAME}_lib  that contains the game logic
# 2) An EXECUTABLE named ${GAME_NAME} which depends on ${GAME_NAME}_lib and the Engine
#

project(${GAME_NAME} LANGUAGES CXX)

# 1) Gather all source/header files
file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

# 2) Create the Game library (so that the Editor can also link to "Game logic")
add_library(${GAME_NAME}_lib STATIC ${GAME_SOURCES})
target_include_directories(${GAME_NAME}_lib
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

# 3) Link with our Engine library
target_link_libraries(${GAME_NAME}_lib
    PUBLIC
        ${ENGINE_NAME}
)

# 4) Now create the final executable for launching the game
#    This can be minimal (just a main() that calls into the library).
#    We'll have a small .cpp in src/ that includes "Game.hpp" and runs something.
add_executable(${GAME_NAME} "${CMAKE_CURRENT_LIST_DIR}/src/GameMain.cpp")
target_link_libraries(${GAME_NAME}
    PRIVATE
        ${GAME_NAME}_lib
)

#
# -- End of Game/CMakeLists.txt --
#
