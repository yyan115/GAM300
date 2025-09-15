# Force CMake to reconfigure when source directory changes
file(GLOB_RECURSE SOURCE_DEPS "src/*" "include/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE_DEPS})

# Automatically find all source files recursively
file(GLOB_RECURSE ENGINE_SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE ENGINE_HEADERS
    "include/*.h"
    "include/*.hpp"
    "src/*.h"
    "src/*.hpp"
)

message(STATUS "Building Engine as shared library for ${CMAKE_BUILD_TYPE}")

# Engine is ALWAYS a shared library
add_library(Engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Define ENGINE_EXPORTS when building the shared library
target_compile_definitions(Engine PRIVATE ENGINE_EXPORTS)

# Set symbol visibility for Linux
set_target_properties(Engine PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

# Automatically include all subdirectories
file(GLOB_RECURSE ENGINE_INCLUDE_SUBDIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
)
set(ENGINE_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
foreach(item ${ENGINE_INCLUDE_SUBDIRS})
    if(IS_DIRECTORY ${item})
        list(APPEND ENGINE_INCLUDE_DIRS ${item})
    endif()
endforeach()

# Find vcpkg packages - must be called after vcpkg toolchain is set
find_package(assimp CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)

# Manual libraries that aren't available in vcpkg or need custom versions
target_include_directories(Engine
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ENGINE_INCLUDE_DIRS}
        # Keep manual libraries: ImGui (docking), FMOD (commercial)
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/include
)

# Link vcpkg libraries and system libraries
target_link_libraries(Engine
    PUBLIC
        ${OPENGL_LIBRARIES}
        # Public dependencies that consumers need
        glm::glm
    PRIVATE
        # Private dependencies only Engine needs
        assimp::assimp
        glfw
        Freetype::Freetype
        glad::glad
        spdlog::spdlog
        rapidjson
        # System libraries
        ${CMAKE_DL_LIBS}
        pthread
)

# Set rpath so executables can find this shared library
set_target_properties(Engine PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "\$ORIGIN"
)