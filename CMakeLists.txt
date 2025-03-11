cmake_minimum_required(VERSION 3.16)

#
# 1) Set up global project name and version.
#    - Define variables for the subproject names so you can easily rename them.
#
set(PROJECT_NAME    "GAM300")
set(ENGINE_NAME     "Engine")
set(GAME_NAME       "Game")
set(EDITOR_NAME     "Editor")
project(${PROJECT_NAME} LANGUAGES CXX)

#
# 2) Set the C++ standard globally (e.g., C++17).
#    - Using 'standard required' to enforce it strictly.
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# 3) Define custom build configurations:
#    - Debug, Release, EditorDebug, EditorRelease
#    - If you are on Linux or CLion, you might not see these 4 configs automatically,
#      but on Windows/Visual Studio, these show up in the dropdown.
#
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;EditorDebug;EditorRelease" CACHE STRING "" FORCE)

# If no build type is specified, default to Debug.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING 
        "Choose build type: Debug, Release, EditorDebug, EditorRelease" FORCE)
endif()

#
# 4) Optionally set compiler warnings
#
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#
# 5) Add the subdirectories for each subproject.
#    - This will create three entries (Engine, Game, Editor) in the solution.
#
add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)

#
# 6) Copying Resources folder into the build directory so the executables can run
#    in-place with all required assets. 'ALL' ensures it runs in every build.
#
add_custom_target(CopyResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "${CMAKE_BINARY_DIR}/Resources"
    COMMENT "Copying game resources to build directory"
)

########################################################################
# ✅ Force Move ALL_BUILD, ZERO_CHECK, and Other CMake Targets Out of Main View
########################################################################

# Ensure Visual Studio folders are enabled
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Move all CMake-generated helper targets (`ALL_BUILD`, `ZERO_CHECK`, `INSTALL`) into "CMakePredefinedTargets"
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakePredefinedTargets")

# Also move CopyResources to "CMakePredefinedTargets" so it's not in the main solution view
set_target_properties(CopyResources PROPERTIES FOLDER "CMakePredefinedTargets")

########################################################################

#
# 7) (Optional) Set the default startup project in Visual Studio to Game
#
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${GAME_NAME})

#
# 8) Place compiled binaries into the 'build/' folder
#    so we don't get extra folders for each subproject.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

#
# -- End of root CMakeLists.txt --
#
