cmake_minimum_required(VERSION 3.20)

# ------------------------------------------------------------------------------
# 1) Collect all .cpp/.c/.hpp/.h in the Game folder
# ------------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE GAME_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# ------------------------------------------------------------------------------
# 2) Declare an executable target for the Game
# ------------------------------------------------------------------------------
add_executable(${GAME_NAME}
    ${GAME_SOURCES}
    ${GAME_HEADERS}
)

# ------------------------------------------------------------------------------
# 3) Link with the Engine library (so the Game can call engine functions)
# ------------------------------------------------------------------------------
target_link_libraries(${GAME_NAME}
    PRIVATE
        ${ENGINE_NAME}
)

# ------------------------------------------------------------------------------
# 4) Include path for Game-specific headers
# ------------------------------------------------------------------------------
target_include_directories(${GAME_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ------------------------------------------------------------------------------
# 5) Automatically copy the Resources/ folder next to the built EXE
# ------------------------------------------------------------------------------
# This ensures that when you build, the EXE has the resources it needs right away.
add_custom_command(
    TARGET ${GAME_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "$<TARGET_FILE_DIR:${GAME_NAME}>/Resources"
)

# End of Game CMakeLists.
