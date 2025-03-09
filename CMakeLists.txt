cmake_minimum_required(VERSION 3.10)
project(GAM300)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix OpenGL preference to avoid CMP0072 warning
cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE "GLVND")

# Detect OS
if(WIN32)
    set(OS_WINDOWS TRUE)
elseif(UNIX)
    set(OS_LINUX TRUE)
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Add source files
add_executable(GAM300 main.cpp)

# Platform-Specific Configurations
if(OS_LINUX)
    message(STATUS "Configuring for Linux...")

    # Manually specify GLEW and GLFW paths since find_package is unreliable in Flatpak
    set(GLEW_INCLUDE_DIR "/usr/include")
    set(GLEW_LIBRARY "/usr/lib64/libGLEW.so")

    set(GLFW_INCLUDE_DIR "/usr/include")
    set(GLFW_LIBRARY "/usr/lib64/libglfw.so")

    # Add include directories
    include_directories(${GLEW_INCLUDE_DIR} ${GLFW_INCLUDE_DIR} /usr/include/GL)

    # Link system libraries
    target_link_libraries(GAM300
            OpenGL::GL
            ${GLEW_LIBRARY}
            ${GLFW_LIBRARY}
    )

elseif(OS_WINDOWS)
    message(STATUS "Configuring for Windows...")

    # Define external library directory (Modify this path if needed)
    set(EXTERN_DIR "${CMAKE_SOURCE_DIR}/extern")

    # Include directories for external dependencies
    include_directories(${EXTERN_DIR}/include)

    # Link Windows libraries
    target_link_libraries(GAM300
            OpenGL32
            ${EXTERN_DIR}/lib/glew32.lib
            ${EXTERN_DIR}/lib/glfw3.lib
    )

    # Copy DLLs after build (so the exe can run directly)
    add_custom_command(TARGET GAM300 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${EXTERN_DIR}/bin/glew32.dll
            ${EXTERN_DIR}/bin/glfw3.dll
            $<TARGET_FILE_DIR:GAM300>
    )
endif()
