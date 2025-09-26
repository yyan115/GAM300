# Force CMake to reconfigure when source directory changes
file(GLOB_RECURSE SOURCE_DEPS "src/*" "include/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE_DEPS})

# Automatically find all source files recursively
file(GLOB_RECURSE ENGINE_SOURCES
    "src/*.cpp"
    "src/*.c"
)

# Remove unused/broken source files
list(REMOVE_ITEM ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/RenderSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TestScene.cpp"
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

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(Engine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

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

# Platform-specific dependencies
if(ANDROID)
    # Android build - using prebuilt libraries
    message(STATUS "Android build - using prebuilt libraries")
    
    # Define Android library paths
    set(ANDROID_LIBS_DIR "${CMAKE_SOURCE_DIR}/Libraries/android/arm64")
    set(ANDROID_HEADERS_DIR "${CMAKE_SOURCE_DIR}/Libraries/android/headers")
    
    # Header-only libraries - include directories only
    set(GLM_INCLUDE_DIRS "${ANDROID_HEADERS_DIR}" CACHE STRING "GLM include directory")
    set(RAPIDJSON_INCLUDE_DIRS "${ANDROID_HEADERS_DIR}/rapidjson" CACHE STRING "RapidJSON include directory")
    
    # Prebuilt static/shared libraries - use direct paths for reliability
    set(SPDLOG_LIBRARY "${ANDROID_LIBS_DIR}/libspdlog.a")
    set(FREETYPE_LIBRARY "${ANDROID_LIBS_DIR}/libfreetype.a")
    set(ASSIMP_LIBRARY "${ANDROID_LIBS_DIR}/libassimp.so")
    set(FMOD_LIBRARY "${ANDROID_LIBS_DIR}/libfmod.so")

    # Verify all library files exist
    if(NOT EXISTS ${SPDLOG_LIBRARY})
        message(FATAL_ERROR "spdlog library not found: ${SPDLOG_LIBRARY}")
    endif()
    if(NOT EXISTS ${FREETYPE_LIBRARY})
        message(FATAL_ERROR "freetype library not found: ${FREETYPE_LIBRARY}")
    endif()
    if(NOT EXISTS ${ASSIMP_LIBRARY})
        message(FATAL_ERROR "assimp library not found: ${ASSIMP_LIBRARY}")
    endif()
    if(NOT EXISTS ${FMOD_LIBRARY})
        message(FATAL_ERROR "fmod library not found: ${FMOD_LIBRARY}")
    endif()

    message(STATUS "Found Android prebuilt libraries:")
    message(STATUS "  spdlog: ${SPDLOG_LIBRARY}")
    message(STATUS "  freetype: ${FREETYPE_LIBRARY}")
    message(STATUS "  assimp: ${ASSIMP_LIBRARY}")
    message(STATUS "  fmod: ${FMOD_LIBRARY}")
else()
    # Desktop builds (Linux/Windows) use vcpkg packages
    find_package(assimp CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(glad CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    find_package(gli CONFIG REQUIRED)
endif()

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

# Add Android-specific headers (vcpkg handles desktop headers automatically)
if(ANDROID)
    target_include_directories(Engine
        PUBLIC
            ${ANDROID_HEADERS_DIR}  # Base headers directory
            ${ANDROID_HEADERS_DIR}/glm        # GLM specific path
            ${ANDROID_HEADERS_DIR}/rapidjson  # RapidJSON specific path
            ${ANDROID_HEADERS_DIR}/assimp     # Assimp specific path
            ${ANDROID_HEADERS_DIR}/spdlog     # spdlog specific path
            ${ANDROID_HEADERS_DIR}/freetype2  # freetype specific path
            ${ANDROID_HEADERS_DIR}/fmod       # FMOD headers for Android
            ${ANDROID_HEADERS_DIR}/GLI        # GLI headers for Android
    )
endif()

# Platform-specific include directories section removed - now handled above

# Platform-specific linking
if(ANDROID)
    # Android linking - using prebuilt libraries
    target_link_libraries(Engine
        PUBLIC
            ${OPENGL_LIBRARIES}
            # GLM and RapidJSON are header-only, no linking needed
        PRIVATE
            # Prebuilt libraries
            ${SPDLOG_LIBRARY}
            ${FREETYPE_LIBRARY}
            ${ASSIMP_LIBRARY}
            ${FMOD_LIBRARY}
            # Android system libraries
            log  # Android log library
            android  # Android NDK library
    )
else()
    # Desktop linking (Linux/Windows) - vcpkg dependencies
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
            gli
    )

    # Platform-specific FMOD and system libraries
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Find FMOD library - try different possible names
        find_library(FMOD_LIBRARY
            NAMES fmod libfmod.so.14.9 libfmod.so.14 libfmod.so
            PATHS ${CMAKE_SOURCE_DIR}/Libraries/FMOD/lib/x64
            NO_DEFAULT_PATH
        )
        if(FMOD_LIBRARY)
            target_link_libraries(Engine PRIVATE
                ${CMAKE_DL_LIBS}
                pthread
                ${FMOD_LIBRARY}
            )
            message(STATUS "Found FMOD library: ${FMOD_LIBRARY}")
        else()
            message(FATAL_ERROR "FMOD library not found")
        endif()
    elseif(WIN32)
        target_link_libraries(Engine PRIVATE
            ${CMAKE_SOURCE_DIR}/Libraries/FMOD/lib/x64/fmod_vc.lib
        )
    endif()
endif()

# Set rpath so executables can find this shared library
set_target_properties(Engine PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "\$ORIGIN:${CMAKE_SOURCE_DIR}/Libraries/FMOD/lib/x64"
)