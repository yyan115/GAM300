cmake_minimum_required(VERSION 3.20)

# ------------------------------------------------------------------------------
# 1) Collect all Editor .cpp/.hpp
# ------------------------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE EDITOR_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# ------------------------------------------------------------------------------
# 2) Declare an executable for the Editor
# ------------------------------------------------------------------------------
add_executable(${EDITOR_NAME}
    ${EDITOR_SOURCES}
    ${EDITOR_HEADERS}
)

# ------------------------------------------------------------------------------
# 3) Link with the Engine library.
#    If you truly need to reference "Game" code from the Editor, you'd have to
#    restructure the Game as a library, then link that here as well.
# ------------------------------------------------------------------------------
target_link_libraries(${EDITOR_NAME}
    PRIVATE
        ${ENGINE_NAME}
        # If the Editor needs the Game as a library, do it here.
)

# ------------------------------------------------------------------------------
# 4) Include path for Editor-specific headers
# ------------------------------------------------------------------------------
target_include_directories(${EDITOR_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ------------------------------------------------------------------------------
# 5) Copy resources to Editor build folder too (if you want them there)
# ------------------------------------------------------------------------------
add_custom_command(
    TARGET ${EDITOR_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "$<TARGET_FILE_DIR:${EDITOR_NAME}>/Resources"
)

# End of Editor CMakeLists.
