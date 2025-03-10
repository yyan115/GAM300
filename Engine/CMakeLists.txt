# We generally do NOT call "project(...)" in sub CMake files if we want a single top-level project.
cmake_minimum_required(VERSION 3.20)

# ------------------------------------------------------------------------------
# 1) Collect the Engine source/header files
# ------------------------------------------------------------------------------
# We recursively grab all .cpp/.c and .hpp/.h in src/ and include/.
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE ENGINE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# ------------------------------------------------------------------------------
# 2) Declare the engine library
# ------------------------------------------------------------------------------
# Using STATIC library. If you want a DLL, change to SHARED.
add_library(${ENGINE_NAME} STATIC
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

# ------------------------------------------------------------------------------
# 3) Include paths (where to find engine headers + third-party libs)
# ------------------------------------------------------------------------------
target_include_directories(${ENGINE_NAME}
    PUBLIC
        # Engine's own "public" include directory
        "${CMAKE_CURRENT_SOURCE_DIR}/include"

        # Example includes for third-party libraries:
        "${CMAKE_CURRENT_SOURCE_DIR}/libraries/glm"
        "${CMAKE_CURRENT_SOURCE_DIR}/libraries/glew/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/libraries/glfw/include"
)

# ------------------------------------------------------------------------------
# 4) Linking to external libraries (GLFW, GLEW) if needed
# ------------------------------------------------------------------------------
# If you have .lib or .a files inside your libraries folder, you can do:
#   find_library(GLFW_LIB glfw3 HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/glfw/lib")
#   target_link_libraries(${ENGINE_NAME} PUBLIC ${GLFW_LIB})
#
# Or if you have a CMake project for GLFW in libraries/glfw:
#   add_subdirectory(libraries/glfw)
#   target_link_libraries(${ENGINE_NAME} PUBLIC glfw)
#
# Same logic for GLEW (or any other library).
# For GLM, typically no need to link; it's headers only.

# ------------------------------------------------------------------------------
# 5) Additional compiler definitions or options (if needed)
# ------------------------------------------------------------------------------
# e.g.: target_compile_definitions(${ENGINE_NAME} PRIVATE MY_ENGINE_INTERNALS=1)

# That's all for the Engine.
