# ------------------------------------------------------------------------------
# Editor/CMakeLists.txt
# This file builds the IMGUI-based editor, which depends on both the engine and game.
# ------------------------------------------------------------------------------
set(EDITOR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(EDITOR_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ------------------------------------------------------------------------------
# Gather Source Files
# ------------------------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SOURCES ${EDITOR_SRC_DIR}/*.cpp)
file(GLOB_RECURSE EDITOR_HEADERS ${EDITOR_INCLUDE_DIR}/*.h)

# ------------------------------------------------------------------------------
# Create the Editor Executable
# ------------------------------------------------------------------------------
add_executable(${EDITOR_PROJECT_NAME} ${EDITOR_SOURCES} ${EDITOR_HEADERS})

# ------------------------------------------------------------------------------
# Specify Include Directories for the Editor
# ------------------------------------------------------------------------------
target_include_directories(${EDITOR_PROJECT_NAME}
    PRIVATE
        ${EDITOR_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/Engine/include  # Engine header dependency
        ${CMAKE_SOURCE_DIR}/Game/include    # Game header dependency (if needed)
)

# ------------------------------------------------------------------------------
# Link the Engine and Game Libraries to the Editor
# ------------------------------------------------------------------------------
target_link_libraries(${EDITOR_PROJECT_NAME}
    PRIVATE
        ${ENGINE_PROJECT_NAME}
        ${GAME_PROJECT_NAME}
)

# ------------------------------------------------------------------------------
# Post-Build: Copy Resources Folder
# ------------------------------------------------------------------------------
add_custom_command(TARGET ${EDITOR_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Resources $<TARGET_FILE_DIR:${EDITOR_PROJECT_NAME}>/Resources
    COMMENT "Copying Resources to output directory for editor"
)
