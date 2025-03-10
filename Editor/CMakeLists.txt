#
# CMakeLists for the Editor subproject
#
# We'll build the Editor as an executable that depends on:
#  - The Engine library (for engine functionality)
#  - The Game library (so the editor can manipulate or introspect game objects)
#

project(${EDITOR_NAME} LANGUAGES CXX)

# 1) Gather all Editor source/header files
file(GLOB_RECURSE EDITOR_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

# 2) Create the Editor executable
add_executable(${EDITOR_NAME} ${EDITOR_SOURCES})

# 3) Link with Engine library and with the Game library (not the Game executable)
target_link_libraries(${EDITOR_NAME}
    PRIVATE
        ${ENGINE_NAME}
        ${GAME_NAME}_lib
)

target_include_directories(${EDITOR_NAME}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

#
# -- End of Editor/CMakeLists.txt --
#
