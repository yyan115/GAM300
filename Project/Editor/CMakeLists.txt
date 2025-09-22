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
)

# Link to Engine (shared lib) and Game (static lib)
target_link_libraries(Editor
    PRIVATE
        Engine  # shared library
        Game    # static library
)

# Copy resources to editor output directory
add_custom_command(TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Resources
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Resources
)

# Windows-specific: Copy required DLLs for Editor
if(WIN32)
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Libraries/assimp/bin/x64/assimp-vc143-mt.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()