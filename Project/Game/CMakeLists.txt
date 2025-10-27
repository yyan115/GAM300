# Force CMake to reconfigure when source directory changes
file(GLOB_RECURSE SOURCE_DEPS "src/*" "include/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE_DEPS})

# Automatically find all source files recursively
file(GLOB_RECURSE GAME_SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE GAME_HEADERS
    "include/*.h"
    "include/*.hpp"
    "src/*.h"
    "src/*.hpp"
)

# Find main.cpp
set(GAME_MAIN_SOURCE "")
foreach(source ${GAME_SOURCES})
    if(source MATCHES ".*main\\.cpp$")
        set(GAME_MAIN_SOURCE ${source})
        break()
    endif()
endforeach()

# Determine build type based on configuration
if(CMAKE_BUILD_TYPE MATCHES "^Editor" OR ANDROID)
    # Editor configs or Android: Game as static library (no main.cpp)
    if(GAME_MAIN_SOURCE)
        list(REMOVE_ITEM GAME_SOURCES ${GAME_MAIN_SOURCE})
        message(STATUS "Building Game as static library (removed main.cpp)")
    endif()
    add_library(Game STATIC ${GAME_SOURCES} ${GAME_HEADERS})
else()
    # Game configs (Debug/Release): Game as executable
    message(STATUS "Building Game as executable")
    add_executable(Game ${GAME_SOURCES} ${GAME_HEADERS})
endif()

# C++ Standard for Game - IMPORTANT: Game uses C++17, not C++20!
# This matches Visual Studio .vcxproj configuration
set_target_properties(Game PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Configuration-specific preprocessor definitions matching Visual Studio
target_compile_definitions(Game PRIVATE
    $<$<CONFIG:Debug>:_DEBUG;_CONSOLE;JPH_PROFILE_ENABLED>
    $<$<CONFIG:Release>:NDEBUG;_CONSOLE>
    $<$<CONFIG:EditorDebug>:_DEBUG;_CONSOLE;EDITOR=1;JPH_DEBUG_RENDERER>
    $<$<CONFIG:EditorRelease>:NDEBUG;_CONSOLE;EDITOR>
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(Game PRIVATE WIN32)
endif()

# Include directories
target_include_directories(Game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # Engine include comes automatically from linking to Engine target
)

# Link to Engine shared library
target_link_libraries(Game
    PRIVATE
        Engine
)

# Post-build commands for executable builds (copy resources + DLLs)
if(NOT CMAKE_BUILD_TYPE MATCHES "^Editor")
    # Copy Resources folder
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Resources
            $<TARGET_FILE_DIR:Game>/Resources
        COMMENT "Copying Resources folder"
    )

    # Platform-specific: Copy required DLLs/shared libraries
    if(WIN32)
        # Copy assimp DLL (check both possible locations)
        if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/Assimp/bin/x64/assimp-vc143-mt.dll")
            add_custom_command(TARGET Game POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/Libraries/Assimp/bin/x64/assimp-vc143-mt.dll
                    $<TARGET_FILE_DIR:Game>
                COMMENT "Copying assimp DLL"
            )
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll")
            add_custom_command(TARGET Game POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll
                    $<TARGET_FILE_DIR:Game>
                COMMENT "Copying assimp DLL"
            )
        endif()

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Copy FMOD .so (if not already copied by Engine)
        if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/libfmod.so.14.9")
            add_custom_command(TARGET Game POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/libfmod.so.14.9
                    $<TARGET_FILE_DIR:Game>
                COMMENT "Copying FMOD .so"
            )
        endif()
    endif()
endif()

# Print Game configuration summary
message(STATUS "----------------------------------------")
message(STATUS "Game Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++17")
if(CMAKE_BUILD_TYPE MATCHES "^Editor" OR ANDROID)
    message(STATUS "  Target Type: Static Library")
else()
    message(STATUS "  Target Type: Executable")
endif()
message(STATUS "----------------------------------------")
