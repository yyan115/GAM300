# ------------------------------------------------------------------------------
# Game/CMakeLists.txt
# This file builds the game executable that uses the engine.
# ------------------------------------------------------------------------------
set(GAME_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(GAME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ------------------------------------------------------------------------------
# Gather Source Files
# ------------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES ${GAME_SRC_DIR}/*.cpp)
file(GLOB_RECURSE GAME_HEADERS ${GAME_INCLUDE_DIR}/*.h)

# ------------------------------------------------------------------------------
# Create the Game Executable
# ------------------------------------------------------------------------------
add_executable(${GAME_PROJECT_NAME} ${GAME_SOURCES} ${GAME_HEADERS})

# ------------------------------------------------------------------------------
# Specify Include Directories for the Game
# ------------------------------------------------------------------------------
target_include_directories(${GAME_PROJECT_NAME}
    PRIVATE
        ${GAME_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/Engine/include  # Engine header dependency
)

# ------------------------------------------------------------------------------
# Link the Engine Library to the Game
# ------------------------------------------------------------------------------
target_link_libraries(${GAME_PROJECT_NAME}
    PRIVATE
        ${ENGINE_PROJECT_NAME}
)

# ------------------------------------------------------------------------------
# Post-Build: Copy Resources Folder
# ------------------------------------------------------------------------------
# This command ensures that after building the game executable, the Resources
# folder is copied to its output directory.
add_custom_command(TARGET ${GAME_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Resources $<TARGET_FILE_DIR:${GAME_PROJECT_NAME}>/Resources
    COMMENT "Copying Resources to output directory for game"
)
