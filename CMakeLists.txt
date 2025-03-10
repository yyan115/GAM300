cmake_minimum_required(VERSION 3.16)

#
# 1) Set up global project name and version
#    - We'll also define variables for the subproject names so you can easily rename them.
#
set(PROJECT_NAME        "GAM300")
set(ENGINE_NAME         "Engine")
set(GAME_NAME           "Game")
set(EDITOR_NAME         "Editor")
project(${PROJECT_NAME} LANGUAGES CXX)

#
# 2) Set the C++ standard globally (e.g., C++17).
#    - Using 'standard required' to enforce it strictly.
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# 3) Optionally set any default compiler warnings if you like.
#    - e.g., for MSVC: /W4 ; for GCC/Clang: -Wall -Wextra -Wpedantic
#
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#
# 4) Add the subdirectories for each subproject.
#    - This will create three entries (Engine, Game, Editor) in the solution.
#
add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)

#
# 5) Copying Resources folder into the build directory so the executables can run
#    in-place with all required assets.
#    - 'ALL' ensures it runs as part of a normal build, not just on 'make install'.
#
add_custom_target(CopyResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "${CMAKE_BINARY_DIR}/Resources"
    COMMENT "Copying game resources to build directory"
)
set_target_properties(CopyResources PROPERTIES FOLDER "CMakePredefinedTargets")  # Hide it

# Also hide the ALL_BUILD and ZERO_CHECK targets
set_target_properties(ALL_BUILD PROPERTIES FOLDER "CMakePredefinedTargets")
set_target_properties(ZERO_CHECK PROPERTIES FOLDER "CMakePredefinedTargets")

#
# 6) Optionally, we can make the 'Game' project the default startup project in Visual Studio
#    by using a custom property. If you prefer the Editor to be the default, set it there.
#    This is purely optional and can vary based on personal preference.
#
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${GAME_NAME})

#
# -- End of root CMakeLists.txt --
#
