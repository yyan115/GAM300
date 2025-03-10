# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.30

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/THDD/Nextcloud/School/Repos/GAM300

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/THDD/Nextcloud/School/Repos/GAM300/build

# Include any dependencies generated for this target.
include Game/CMakeFiles/Game.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include Game/CMakeFiles/Game.dir/compiler_depend.make

# Include the progress variables for this target.
include Game/CMakeFiles/Game.dir/progress.make

# Include the compile flags for this target's objects.
include Game/CMakeFiles/Game.dir/flags.make

Game/CMakeFiles/Game.dir/src/Game.cpp.o: Game/CMakeFiles/Game.dir/flags.make
Game/CMakeFiles/Game.dir/src/Game.cpp.o: /mnt/THDD/Nextcloud/School/Repos/GAM300/Game/src/Game.cpp
Game/CMakeFiles/Game.dir/src/Game.cpp.o: Game/CMakeFiles/Game.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/mnt/THDD/Nextcloud/School/Repos/GAM300/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object Game/CMakeFiles/Game.dir/src/Game.cpp.o"
	cd /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT Game/CMakeFiles/Game.dir/src/Game.cpp.o -MF CMakeFiles/Game.dir/src/Game.cpp.o.d -o CMakeFiles/Game.dir/src/Game.cpp.o -c /mnt/THDD/Nextcloud/School/Repos/GAM300/Game/src/Game.cpp

Game/CMakeFiles/Game.dir/src/Game.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/Game.dir/src/Game.cpp.i"
	cd /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mnt/THDD/Nextcloud/School/Repos/GAM300/Game/src/Game.cpp > CMakeFiles/Game.dir/src/Game.cpp.i

Game/CMakeFiles/Game.dir/src/Game.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/Game.dir/src/Game.cpp.s"
	cd /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mnt/THDD/Nextcloud/School/Repos/GAM300/Game/src/Game.cpp -o CMakeFiles/Game.dir/src/Game.cpp.s

# Object files for target Game
Game_OBJECTS = \
"CMakeFiles/Game.dir/src/Game.cpp.o"

# External object files for target Game
Game_EXTERNAL_OBJECTS =

Game/Game: Game/CMakeFiles/Game.dir/src/Game.cpp.o
Game/Game: Game/CMakeFiles/Game.dir/build.make
Game/Game: Game/libGame_lib.a
Game/Game: Engine/libEngine.a
Game/Game: /mnt/THDD/Nextcloud/School/Repos/GAM300/Engine/libraries/glew/lib/libGLEW.so
Game/Game: /mnt/THDD/Nextcloud/School/Repos/GAM300/Engine/libraries/glfw/lib/libglfw3.a
Game/Game: Game/CMakeFiles/Game.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/mnt/THDD/Nextcloud/School/Repos/GAM300/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable Game"
	cd /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Game.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
Game/CMakeFiles/Game.dir/build: Game/Game
.PHONY : Game/CMakeFiles/Game.dir/build

Game/CMakeFiles/Game.dir/clean:
	cd /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game && $(CMAKE_COMMAND) -P CMakeFiles/Game.dir/cmake_clean.cmake
.PHONY : Game/CMakeFiles/Game.dir/clean

Game/CMakeFiles/Game.dir/depend:
	cd /mnt/THDD/Nextcloud/School/Repos/GAM300/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/THDD/Nextcloud/School/Repos/GAM300 /mnt/THDD/Nextcloud/School/Repos/GAM300/Game /mnt/THDD/Nextcloud/School/Repos/GAM300/build /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game /mnt/THDD/Nextcloud/School/Repos/GAM300/build/Game/CMakeFiles/Game.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : Game/CMakeFiles/Game.dir/depend

