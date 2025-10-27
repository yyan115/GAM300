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

# Add ImGui source files for Editor (docking branch)
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

# C++ Standard for Editor
set_target_properties(Editor PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Configuration-specific preprocessor definitions matching Visual Studio
target_compile_definitions(Editor PRIVATE
    $<$<CONFIG:Debug>:_DEBUG;_CONSOLE;JPH_PROFILE_ENABLED>
    $<$<CONFIG:Release>:NDEBUG;_CONSOLE>
    $<$<CONFIG:EditorDebug>:_DEBUG;_CONSOLE;EDITOR;JPH_PROFILE_ENABLED>
    $<$<CONFIG:EditorRelease>:NDEBUG;_CONSOLE;EDITOR>
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(Editor PRIVATE WIN32)
endif()

# Include directories
target_include_directories(Editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/Engine/include
        ${CMAKE_SOURCE_DIR}/Game/include
        # Manual libraries for Editor-specific features
        ${CMAKE_SOURCE_DIR}/Libraries/ImGui
        ${CMAKE_SOURCE_DIR}/Libraries/ImGui/backends
        ${CMAKE_SOURCE_DIR}/Libraries/FileWatch
        ${CMAKE_SOURCE_DIR}/Libraries/IconFontCppHeaders
        # FMOD headers needed by Engine dependencies
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/inc
)

# Link to Engine (shared lib) and Game (static lib)
target_link_libraries(Editor
    PRIVATE
        Engine  # shared library
        Game    # static library
)

# Platform-specific linking
if(WIN32)
    # Additional Windows libraries for Editor
    # Note: ImGui sources are compiled directly into Editor, no separate lib needed
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Additional Linux libraries if needed
endif()

# Post-build: Copy resources to editor output directory
add_custom_command(TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:Editor>/Resources
    COMMENT "Copying Resources folder"
)

# Platform-specific: Copy required DLLs/shared libraries for Editor
if(WIN32)
    # Copy assimp DLL (check both possible locations)
    if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/Assimp/bin/x64/assimp-vc143-mt.dll")
        add_custom_command(TARGET Editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/Libraries/Assimp/bin/x64/assimp-vc143-mt.dll
                $<TARGET_FILE_DIR:Editor>
            COMMENT "Copying assimp DLL"
        )
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll")
        add_custom_command(TARGET Editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll
                $<TARGET_FILE_DIR:Editor>
            COMMENT "Copying assimp DLL"
        )
    endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Copy FMOD .so (if not already copied by Engine)
    if(EXISTS "${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/libfmod.so.14.9")
        add_custom_command(TARGET Editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/libfmod.so.14.9
                $<TARGET_FILE_DIR:Editor>
            COMMENT "Copying FMOD .so"
        )
    endif()
endif()

# Print Editor configuration summary
message(STATUS "----------------------------------------")
message(STATUS "Editor Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++20")
message(STATUS "  ImGui: Docking branch (compiled from source)")
message(STATUS "  Target Type: Executable")
message(STATUS "----------------------------------------")
