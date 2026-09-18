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

    # ============================================================================
    # CONSOLE WINDOW TOGGLE
    # ============================================================================
    # Release is built with the console on purpose, because it is what gets
    # debugged. What must not ship with a console is the installer, and that
    # used to mean editing this file before every release build.
    #
    # So it is an option instead: on by default, and the release workflow
    # configures with -DGAME_CONSOLE=OFF. Nothing here changes between a local
    # Release build and a shipping one except that flag.
    #
    # WIN32_EXECUTABLE sets the linker subsystem to WINDOWS, which is what
    # actually removes the console. /ENTRY:mainCRTStartup goes with it because
    # the entry point is main(), not WinMain(), and the WINDOWS subsystem looks
    # for WinMain by default.
    # ============================================================================
    option(GAME_CONSOLE "Show the console window in the Game executable" ON)

    add_executable(Game ${GAME_SOURCES} ${GAME_HEADERS} ${APP_ICON_RESOURCE})

    if(NOT WIN32)
        message(STATUS "Game: console option does not apply on this platform")
    elseif(GAME_CONSOLE)
        message(STATUS "Game: console ENABLED (CUI subsystem)")
        target_compile_definitions(Game PRIVATE GAME_CONSOLE=1)
    else()
        message(STATUS "Game: console DISABLED (GUI subsystem)")
        set_target_properties(Game PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(Game PRIVATE /ENTRY:mainCRTStartup)
        endif()
        # The subsystem alone is not enough. main() calls AllocConsole when
        # SHOW_CONSOLE is defined, which would hand a GUI-subsystem build a
        # console window at runtime and undo the whole thing.
        target_compile_definitions(Game PRIVATE GAME_CONSOLE=0)
    endif()

    # ============================================================================
    # INSTALLATION RULES
    # ============================================================================
    install(TARGETS Game DESTINATION .)

    # Install Resources folder
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources" DESTINATION .)

    # Install the files that ship beside the game unchanged, such as the
    # licence notices. They are not assets, so they live outside Resources.
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Distribution/" DESTINATION .)

    # Install FMOD DLL
    if(WIN32)
        install(FILES "${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/fmod.dll" DESTINATION .)
    endif()

    # Install vcpkg DLLs (Release)
    if(WIN32)
        install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/" 
                DESTINATION .
                FILES_MATCHING PATTERN "*.dll")
    endif()

endif()

# Rename game executable to Kusane.exe (not applicable to static lib builds)
if(NOT CMAKE_BUILD_TYPE MATCHES "Editor" AND NOT ANDROID)
    set_target_properties(Game PROPERTIES OUTPUT_NAME "Kusane")
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
    # This is a target of its own rather than a POST_BUILD step on Game,
    # because a POST_BUILD step only runs when Game relinks. Editing a Lua
    # script or a scene does not relink anything, so the build reported
    # "no work to do" and left the old copy in the runtime directory: the
    # game then ran the previous version of a script that had already been
    # edited, with nothing anywhere saying so. file(COPY) skips files that
    # are already up to date, so running this every build is cheap.
    add_custom_target(GameResources ALL
        COMMAND ${CMAKE_COMMAND}
            -DSRC="${CMAKE_SOURCE_DIR}/Resources"
            -DDST="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Resources"
            -P "${CMAKE_SOURCE_DIR}/cmake/CopyResources.cmake"
        COMMENT "Syncing Resources into the runtime directory"
    )
    add_dependencies(Game GameResources)

    # Platform-specific: Copy required libraries
    if(WIN32)
        # Copy FMOD DLL
        add_custom_command(TARGET Game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/fmod.dll
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
        # Copy vcpkg DLLs (debug or release based on config)
        add_custom_command(TARGET Game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:EditorDebug>>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>"
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

