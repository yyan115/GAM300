#
# CMakeLists for the Engine subproject
#
# This builds a STATIC library called ${ENGINE_NAME}.
#
# We also assume third-party dependencies (GLM, GLEW, GLFW) are located
# in libraries/ subfolders. Adjust paths as needed for your actual library files.
#

# We want our Engine to be recognized as its own project in Visual Studio:
project(${ENGINE_NAME} LANGUAGES CXX)

# 1) Gather all source/header files automatically via GLOB_RECURSE:
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)
# NOTE: If you add new .cpp/.h files, you may need to re-run CMake to pick them up.

# 2) Create the static library:
add_library(${ENGINE_NAME} STATIC ${ENGINE_SOURCES})

# 3) Set the include paths so that #include "Engine.hpp" will work from inside or outside:
target_include_directories(${ENGINE_NAME}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
        # Example: GLM (header-only)
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glm"
        # Example: GLEW
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew/include"
        # Example: GLFW
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw/include"
)

# 4) Link to the libraries.
#    - For GLEW and GLFW, typically you have .lib files on Windows or .a/.so on Linux.
#    - Adjust these paths/file names to match your environment:
if (MSVC)
    # Example: Link with GLEW and GLFW .lib files.
    # Suppose your .lib files are at libraries/glew/lib/Debug/glew32.lib, etc.
    # You might do something like:
    target_link_directories(${ENGINE_NAME} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew/lib"
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw/lib"
    )
    target_link_libraries(${ENGINE_NAME} PRIVATE glew32 glfw3 opengl32)
else()
    # On Linux, link with .so or .a. The exact library names vary.
    # Also link to dl, pthread, X11, etc. if needed by GLFW. For example:
    target_link_libraries(${ENGINE_NAME} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glew/lib/libGLEW.so"
        "${CMAKE_CURRENT_LIST_DIR}/libraries/glfw/lib/libglfw3.a"
        GL
        X11
        pthread
        dl
    )
endif()

# 5) Set any compiler definitions or special flags if needed for the libraries:
# target_compile_definitions(${ENGINE_NAME} PRIVATE GLEW_STATIC)

#
# -- End of Engine/CMakeLists.txt --
#
