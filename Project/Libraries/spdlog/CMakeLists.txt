cmake_minimum_required(VERSION 3.22.1)
project(spdlog)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure PIC is enabled for static libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add compile flags for Android
if(ANDROID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
endif()

# Include directories
include_directories(include)

# Collect all source files
file(GLOB_RECURSE SPDLOG_SOURCES "src/*.cpp")

# Create static library
add_library(spdlog STATIC ${SPDLOG_SOURCES})

# Set target properties
set_target_properties(spdlog PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Include directories for the target
target_include_directories(spdlog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler-specific options
target_compile_options(spdlog PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

# Install targets
install(TARGETS spdlog
    EXPORT spdlogTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)