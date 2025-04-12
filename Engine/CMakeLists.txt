#
# CMakeLists for the Engine subproject
# Builds a STATIC library called ${ENGINE_NAME}.
# Also links to GLEW/GLFW in "Engine/libraries/".
#

project(${ENGINE_NAME} LANGUAGES CXX)

# 1) Gather all source/header files automatically via GLOB_RECURSE:
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

# 2) Create the static library:
add_library(${ENGINE_NAME} STATIC ${ENGINE_SOURCES})

# 3) Set include directories:
target_include_directories(${ENGINE_NAME}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
        # GLM (headers only)
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glm"
        # GLEW
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew/include"
        # GLFW
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw/include"
)

# 4) Link to GLEW and GLFW. Adjust paths/names for your environment.
if(MSVC)
    target_link_directories(${ENGINE_NAME} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew"
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw"
    )
    # If using static GLEW, define GLEW_STATIC:
    target_compile_definitions(${ENGINE_NAME} PRIVATE GLEW_STATIC)

    # Link against the correct .lib files
    target_link_libraries(${ENGINE_NAME} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew/glew32s.lib"
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw/glfw3.lib"
        opengl32
    )
else()
    # On Linux:
    target_link_libraries(${ENGINE_NAME} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew/libGLEW.so"
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw/libglfw.so"
        GL
        X11
        pthread
        dl
    )
endif()
