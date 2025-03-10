# ------------------------------------------------------------------------------
# Engine/CMakeLists.txt
# This file builds the custom game engine as a library.
# ------------------------------------------------------------------------------
# Define directories for source and header files.
set(ENGINE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ENGINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ------------------------------------------------------------------------------
# Gather Source Files
# ------------------------------------------------------------------------------
# Using file(GLOB_RECURSE ...) is convenient but note that for production builds,
# manually listing source files is recommended.
file(GLOB_RECURSE ENGINE_SOURCES ${ENGINE_SRC_DIR}/*.cpp)
file(GLOB_RECURSE ENGINE_HEADERS ${ENGINE_INCLUDE_DIR}/*.h)

# ------------------------------------------------------------------------------
# Create the Engine Library Target
# ------------------------------------------------------------------------------
if(BUILD_SHARED_ENGINE)
    add_library(${ENGINE_PROJECT_NAME} SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})
else()
    add_library(${ENGINE_PROJECT_NAME} STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
endif()

# ------------------------------------------------------------------------------
# Specify Include Directories for the Engine
# ------------------------------------------------------------------------------
target_include_directories(${ENGINE_PROJECT_NAME}
    PUBLIC
        ${ENGINE_INCLUDE_DIR}
)

# ------------------------------------------------------------------------------
# Link External Dependencies (if the engine uses them)
# ------------------------------------------------------------------------------
target_link_libraries(${ENGINE_PROJECT_NAME}
    PUBLIC
        glm::glm
        glfw
        GLEW::GLEW
)

# ------------------------------------------------------------------------------
# Windows-Specific: DLL Export Definitions
# ------------------------------------------------------------------------------
# When building a shared library on Windows, you might need to define export macros.
if(WIN32 AND BUILD_SHARED_ENGINE)
    target_compile_definitions(${ENGINE_PROJECT_NAME} PRIVATE ENGINE_EXPORTS)
endif()
