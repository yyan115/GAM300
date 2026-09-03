# Libraries/lua/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(lua_local C)

# find the lua source files in this directory's src/
file(GLOB LUA_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Exclude lua.c and luac.c - they are standalone interpreter executables with main() functions
# We only want the Lua library, not the interpreters
list(REMOVE_ITEM LUA_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/luac.c"
)

add_library(lua STATIC ${LUA_SRCS})

# Lua is on Android's frame hot path and is linked directly into the engine.
# Keep its C translation units as ThinLTO bitcode so the final Engine link can
# optimize across the VM's internal API boundaries as well as C++ call sites.
if(ANDROID AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(lua PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-flto=thin>
    )
endif()

# public include points to the Lua headers (they are in the same src folder)
target_include_directories(lua
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# position independent code for Android/shared usage
set_target_properties(lua PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "lua"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:EditorDebug>>:Debug>DLL"
)

# optional compile definitions for Windows
if(MSVC)
    target_compile_definitions(lua PRIVATE LUA_BUILD_AS_DLL=0)
endif()
