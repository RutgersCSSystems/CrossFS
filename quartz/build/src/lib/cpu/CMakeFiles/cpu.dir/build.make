# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /users/skannan/ssd/fsoffload/quartz

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /users/skannan/ssd/fsoffload/quartz/build

# Include any dependencies generated for this target.
include src/lib/cpu/CMakeFiles/cpu.dir/depend.make

# Include the progress variables for this target.
include src/lib/cpu/CMakeFiles/cpu.dir/progress.make

# Include the compile flags for this target's objects.
include src/lib/cpu/CMakeFiles/cpu.dir/flags.make

src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o: src/lib/cpu/CMakeFiles/cpu.dir/flags.make
src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o: ../src/lib/cpu/cpu.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/skannan/ssd/fsoffload/quartz/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o"
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && /usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/cpu.dir/cpu.c.o   -c /users/skannan/ssd/fsoffload/quartz/src/lib/cpu/cpu.c

src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/cpu.dir/cpu.c.i"
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && /usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/skannan/ssd/fsoffload/quartz/src/lib/cpu/cpu.c > CMakeFiles/cpu.dir/cpu.c.i

src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/cpu.dir/cpu.c.s"
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && /usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/skannan/ssd/fsoffload/quartz/src/lib/cpu/cpu.c -o CMakeFiles/cpu.dir/cpu.c.s

src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.requires:

.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.requires

src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.provides: src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.requires
	$(MAKE) -f src/lib/cpu/CMakeFiles/cpu.dir/build.make src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.provides.build
.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.provides

src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.provides.build: src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o


src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o: src/lib/cpu/CMakeFiles/cpu.dir/flags.make
src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o: ../src/lib/cpu/pmc.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/skannan/ssd/fsoffload/quartz/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o"
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && /usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/cpu.dir/pmc.c.o   -c /users/skannan/ssd/fsoffload/quartz/src/lib/cpu/pmc.c

src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/cpu.dir/pmc.c.i"
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && /usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/skannan/ssd/fsoffload/quartz/src/lib/cpu/pmc.c > CMakeFiles/cpu.dir/pmc.c.i

src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/cpu.dir/pmc.c.s"
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && /usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/skannan/ssd/fsoffload/quartz/src/lib/cpu/pmc.c -o CMakeFiles/cpu.dir/pmc.c.s

src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.requires:

.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.requires

src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.provides: src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.requires
	$(MAKE) -f src/lib/cpu/CMakeFiles/cpu.dir/build.make src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.provides.build
.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.provides

src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.provides.build: src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o


cpu: src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o
cpu: src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o
cpu: src/lib/cpu/CMakeFiles/cpu.dir/build.make

.PHONY : cpu

# Rule to build all files generated by this target.
src/lib/cpu/CMakeFiles/cpu.dir/build: cpu

.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/build

src/lib/cpu/CMakeFiles/cpu.dir/requires: src/lib/cpu/CMakeFiles/cpu.dir/cpu.c.o.requires
src/lib/cpu/CMakeFiles/cpu.dir/requires: src/lib/cpu/CMakeFiles/cpu.dir/pmc.c.o.requires

.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/requires

src/lib/cpu/CMakeFiles/cpu.dir/clean:
	cd /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu && $(CMAKE_COMMAND) -P CMakeFiles/cpu.dir/cmake_clean.cmake
.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/clean

src/lib/cpu/CMakeFiles/cpu.dir/depend:
	cd /users/skannan/ssd/fsoffload/quartz/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/skannan/ssd/fsoffload/quartz /users/skannan/ssd/fsoffload/quartz/src/lib/cpu /users/skannan/ssd/fsoffload/quartz/build /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu /users/skannan/ssd/fsoffload/quartz/build/src/lib/cpu/CMakeFiles/cpu.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/lib/cpu/CMakeFiles/cpu.dir/depend

