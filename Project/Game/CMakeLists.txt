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
if(CMAKE_BUILD_TYPE MATCHES "Editor")
    # Editor configs: Game as static library (no main.cpp)
    if(GAME_MAIN_SOURCE)
        list(REMOVE_ITEM GAME_SOURCES ${GAME_MAIN_SOURCE})
        message(STATUS "Building Game as static library (removed main.cpp)")
    endif()
    add_library(Game STATIC ${GAME_SOURCES} ${GAME_HEADERS})
else()
    # Game configs: Game as executable
    message(STATUS "Building Game as executable")
    add_executable(Game ${GAME_SOURCES} ${GAME_HEADERS})
endif()

# Automatically include all subdirectories
file(GLOB_RECURSE GAME_INCLUDE_SUBDIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
)
set(GAME_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
foreach(item ${GAME_INCLUDE_SUBDIRS})
    if(IS_DIRECTORY ${item})
        list(APPEND GAME_INCLUDE_DIRS ${item})
    endif()
endforeach()

target_include_directories(Game
    PRIVATE
        ${GAME_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/Engine/include
)

# Link to Engine shared library
target_link_libraries(Game
    PRIVATE
        Engine
)

# Copy resources (only for executable builds)
if(NOT CMAKE_BUILD_TYPE MATCHES "Editor")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resources
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Resources
    )
    
    # Windows-specific: Copy required DLLs
    if(WIN32)
        add_custom_command(TARGET Game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
    endif()
endif()