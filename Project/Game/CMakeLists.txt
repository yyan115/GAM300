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

# --- Ensure a Lua target exists (prefer package, otherwise use local copy) ---
# Try config-mode package first (vcpkg or installed lua)
find_package(lua CONFIG QUIET)

if(NOT TARGET lua)
    # fallback: if we have a local Libraries/lua CMakeLists, add it
    if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/lua/CMakeLists.txt")
        message(STATUS "Using local Lua at ${CMAKE_SOURCE_DIR}/Libraries/lua")
        add_subdirectory("${CMAKE_SOURCE_DIR}/Libraries/lua")
    else()
        # As a last resort, try old-style find
        find_package(Lua REQUIRED)
    endif()
endif()

# Expose an include dir if the imported target does not provide it
# (this is defensive; if your lua target sets PUBLIC include dirs this is a no-op)
if(TARGET lua)
    # if the target has an include property, linking will propagate it;
    # otherwise explicitly add the local include directory if it exists
    if(NOT TARGET lua::lua AND EXISTS "${CMAKE_SOURCE_DIR}/Libraries/lua/src")
        # This only sets a fallback include for targets that link lua below.
        set(LUA_FALLBACK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Libraries/lua/src")
    endif()
else()
    # If find_package(Lua) used non-config-mode, expose fallback include path
    if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/lua/src")
        set(LUA_FALLBACK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Libraries/lua/src")
    endif()
endif()

# Find main.cpp
set(GAME_MAIN_SOURCE "")
foreach(source ${GAME_SOURCES})
    if(source MATCHES ".*main\\.cpp$")
        set(GAME_MAIN_SOURCE ${source})
        break()
    endif()
endforeach()

# Determine build type based on configuration
if(CMAKE_BUILD_TYPE MATCHES "Editor" OR ANDROID)
    # Editor configs or Android: Game as static library (no main.cpp)
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
        # Engine include comes from linking to Engine target, not manually added
)

# Link to Engine shared library
target_link_libraries(Game
    PRIVATE
        Engine
)

# Link to Lua (safe - works for either static library or executable Game)
if(TARGET lua)
    target_link_libraries(Game PRIVATE lua)
elseif(TARGET lua::lua)
    # vcpkg/config-mode usually provides lua::lua
    target_link_libraries(Game PRIVATE lua::lua)
endif()

# Apply fallback include dir if necessary
if(DEFINED LUA_FALLBACK_INCLUDE_DIR)
    target_include_directories(Game PRIVATE ${LUA_FALLBACK_INCLUDE_DIR})
endif()

# Copy resources (only for executable builds)
if(NOT CMAKE_BUILD_TYPE MATCHES "Editor")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resources
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Resources
    )

    # Platform-specific: Copy required libraries
    if(WIN32)
        add_custom_command(TARGET Game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
        add_custom_command(TARGET Game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Libraries/FMOD/lib/x64/fmod.dll
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_custom_command(TARGET Game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/libfmod.so.14.9
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
    endif()
endif()