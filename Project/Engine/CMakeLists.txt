# Force CMake to reconfigure when source directory changes
file(GLOB_RECURSE SOURCE_DEPS "src/*" "include/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE_DEPS})

# Automatically find all source files recursively
file(GLOB_RECURSE ENGINE_SOURCES
    "src/*.cpp"
    "src/*.c"
)

# Add Scripting sources from Scripting folder
file(GLOB_RECURSE SCRIPTING_SOURCES
    "${CMAKE_SOURCE_DIR}/Scripting/src/*.cpp"
)
list(APPEND ENGINE_SOURCES ${SCRIPTING_SOURCES})

# Remove unused/broken source files
list(REMOVE_ITEM ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/RenderSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TestScene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/allocator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/clusterizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/indexanalyzer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/indexcodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/indexgenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/overdrawoptimizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/partition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/quantization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/rasterizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/simplifier.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/spatialorder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/stripifier.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/vcacheoptimizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/vertexcodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/vertexfilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Mesh Optimizer/vfetchoptimizer.cpp"
)

# Platform-specific source file exclusions
if(ANDROID)
    # Android uses OpenGL ES, not desktop OpenGL - exclude GLAD (desktop OpenGL loader)
    list(REMOVE_ITEM ENGINE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c"
    )
    message(STATUS "Excluded glad.c for Android")
endif()

# Exclude deprecated/unused files from all platforms
list(REMOVE_ITEM ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/Scripting/src/hotload_deprecated.cpp"
)

# Exclude Windows-specific files on non-Windows platforms
if(NOT WIN32)
    list(REMOVE_ITEM ENGINE_SOURCES
        "${CMAKE_SOURCE_DIR}/Scripting/src/ScriptFileSystem_win.cpp"
    )
    message(STATUS "Excluded Windows-specific Scripting files for non-Windows platform")
endif()

file(GLOB_RECURSE ENGINE_HEADERS
    "include/*.h"
    "include/*.hpp"
    "src/*.h"
    "src/*.hpp"
)

# Add Scripting headers from Scripting folder
file(GLOB_RECURSE SCRIPTING_HEADERS
    "${CMAKE_SOURCE_DIR}/Scripting/include/*.h"
    "${CMAKE_SOURCE_DIR}/Scripting/include/*.hpp"
)
list(APPEND ENGINE_HEADERS ${SCRIPTING_HEADERS})

message(STATUS "Building Engine as shared library for ${CMAKE_BUILD_TYPE}")

# Engine is ALWAYS a shared library
add_library(Engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Define ENGINE_EXPORTS when building the shared library
target_compile_definitions(Engine PRIVATE ENGINE_EXPORTS)

# Define EDITOR for Editor builds (EditorDebug, EditorRelease)
if(CMAKE_BUILD_TYPE MATCHES "Editor")
    target_compile_definitions(Engine PRIVATE EDITOR)
endif()

# Jolt Physics compile definitions:
# For Android: Jolt's CMakeLists.txt exports all defines as INTERFACE (via patch in ImportDependencies.cmake)
#              Engine gets them automatically when linking to Jolt
# For Desktop: Defines come from JoltInclude.hpp header

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
    find_package(Jolt CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    find_package(gli CONFIG REQUIRED)
    find_package(meshoptimizer CONFIG REQUIRED)
endif()

# Manual libraries that aren't available in vcpkg or need custom versions
target_include_directories(Engine
    PUBLIC
        # Only expose the base include directory so consumers can do #include "Engine.h"
        # NOT subdirectories - those are internal implementation
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        # Scripting library headers - PUBLIC because used in Engine's public headers
        ${CMAKE_SOURCE_DIR}/Scripting/include
        # Lua headers - PUBLIC because Scripting headers include lua.h
        ${CMAKE_SOURCE_DIR}/Libraries/lua/src
        # hfsm2 library - PUBLIC because used in Engine's public headers
        ${CMAKE_SOURCE_DIR}/Libraries/hfsm2
    PRIVATE
        # Engine internals can see everything
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ENGINE_INCLUDE_DIRS}
        # Keep manual libraries: ImGui (docking), FMOD (commercial)
        ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/inc
        ${CMAKE_SOURCE_DIR}/Libraries/luabridge3/source
)

# Add Compressonator include directory for Editor builds
if(CMAKE_BUILD_TYPE MATCHES "Editor")
    if(WIN32)
        # Windows uses prebuilt binaries
        target_include_directories(Engine PRIVATE
            ${CMAKE_SOURCE_DIR}/Libraries/Compressonator/include
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Linux builds from source - include directory handled by FetchContent target
        # Will be added when we link against Compressonator target
    endif()
endif()

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
    # Android linking - using prebuilt libraries + FetchContent Jolt
    target_link_libraries(Engine
        PUBLIC
            ${OPENGL_LIBRARIES}
            # GLM and RapidJSON are header-only, no linking needed
        PRIVATE
            # FetchContent Jolt Physics - PRIVATE so Game doesn't see it
            Jolt
            # Lua scripting library
            lua
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
    # Desktop linking (Linux/Windows) - vcpkg dependencies + FetchContent Jolt
    target_link_libraries(Engine
        PUBLIC
            ${OPENGL_LIBRARIES}
            # PUBLIC: These are used in Engine's public headers
            glad::glad
            RapidJSON
            glm::glm
        PRIVATE
            # PRIVATE: These are only used internally by Engine
            Jolt::Jolt
            lua
            assimp::assimp
            glfw
            Freetype::Freetype
            spdlog::spdlog
            gli
            meshoptimizer::meshoptimizer
    )

    # Platform-specific FMOD and system libraries
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Find FMOD library - try different possible names
        find_library(FMOD_LIBRARY
            NAMES fmod libfmod.so.14.9 libfmod.so.14 libfmod.so
            PATHS ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64
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
            ${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64/fmod_vc.lib
        )
    endif()

    # Link Compressonator for Editor builds
    if(CMAKE_BUILD_TYPE MATCHES "Editor")
        if(WIN32)
            # Windows: Link prebuilt Compressonator static library (no DLL needed)
            if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "EditorDebug")
                target_link_libraries(Engine PRIVATE
                    ${CMAKE_SOURCE_DIR}/Libraries/Compressonator/lib/bin/x64/Compressonator_MDd.lib
                )
            else()
                target_link_libraries(Engine PRIVATE
                    ${CMAKE_SOURCE_DIR}/Libraries/Compressonator/lib/bin/x64/Compressonator_MD.lib
                )
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            # Linux: Link FetchContent built Compressonator
            if(TARGET Compressonator)
                target_link_libraries(Engine PRIVATE Compressonator)
                message(STATUS "Linked Compressonator from FetchContent for Editor")
            else()
                message(WARNING "Compressonator target not available - Editor texture compression may fail")
            endif()
        endif()
    endif()
endif()

# Set rpath so executables can find this shared library
set_target_properties(Engine PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "\$ORIGIN:${CMAKE_SOURCE_DIR}/Libraries/FMOD/core/lib/x64"
)