# Force CMake to reconfigure when source directory changes
file(GLOB_RECURSE SOURCE_DEPS "src/*" "include/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE_DEPS})

# Automatically find all source files recursively
file(GLOB_RECURSE EDITOR_SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE EDITOR_HEADERS
    "include/*.h"
    "include/*.hpp"
    "src/*.h"
    "src/*.hpp"
)

# Try to find vcpkg/system lua first; otherwise add local Libraries/lua
find_package(lua CONFIG QUIET)

if(NOT TARGET lua)
    if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/lua/CMakeLists.txt")
        message(STATUS "Using local Lua at ${CMAKE_SOURCE_DIR}/Libraries/lua")
        add_subdirectory("${CMAKE_SOURCE_DIR}/Libraries/lua")
    else()
        find_package(Lua REQUIRED) # fallback to system find
    endif()
endif()

# Add ImGui source files for Editor
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/ImGui/ImGuizmo.cpp
)

message(STATUS "Building Editor executable for ${CMAKE_BUILD_TYPE}")

# Editor is always an executable  
add_executable(Editor ${EDITOR_SOURCES} ${EDITOR_HEADERS} ${IMGUI_SOURCES})

# Automatically include all subdirectories
file(GLOB_RECURSE EDITOR_INCLUDE_SUBDIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
)
set(EDITOR_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
foreach(item ${EDITOR_INCLUDE_SUBDIRS})
    if(IS_DIRECTORY ${item})
        list(APPEND EDITOR_INCLUDE_DIRS ${item})
    endif()
endforeach()

target_include_directories(Editor
    PRIVATE
        ${EDITOR_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/Engine/include
        ${CMAKE_SOURCE_DIR}/Game/include
        # Manual libraries for Editor-specific features (more troublesome to set up in vcpkg)
        ${CMAKE_SOURCE_DIR}/Libraries/ImGui
        ${CMAKE_SOURCE_DIR}/Libraries/ImGui/backends
        ${CMAKE_SOURCE_DIR}/Libraries/FileWatch
        # FMOD headers needed by Engine dependencies
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/inc
)

# Link to Engine (shared lib) and Game (static lib)
target_link_libraries(Editor
    PRIVATE
        Engine  # shared library
        Game    # static library
)

# --- Link Lua to Editor (works for local add_subdirectory or vcpkg config package) ---
if(TARGET lua)
    target_link_libraries(Editor PRIVATE lua)
elseif(TARGET lua::lua)
    target_link_libraries(Editor PRIVATE lua::lua)
endif()

# Add include directory fallback if not provided by the imported target
if(DEFINED LUA_FALLBACK_INCLUDE_DIR)
    target_include_directories(Editor PRIVATE ${LUA_FALLBACK_INCLUDE_DIR})
else()
    # If you keep a local copy of Lua source in Libraries/lua/src, add that include as a fallback
    if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/lua/src")
        target_include_directories(Editor PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/lua/src)
    endif()
endif()

# Copy resources to editor output directory
add_custom_command(TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Resources
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Resources
)

# Platform-specific: Copy required libraries for Editor
if(WIN32)
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/lib/x64/fmod.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/lib/x64/libfmod.so.14.9
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()