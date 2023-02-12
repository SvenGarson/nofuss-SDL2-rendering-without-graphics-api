# Source files to compile
OBJS = source/main.c

# Choose compiler
CC = gcc

# Compiler flags
COMPILER_FLAGS = -Wextra -Wall

# Build name and directory
OBJ_NAME = builds/driver

# Specify SDL2 library headers directory
INCLUDE_PATHS = -IC:/sdl2_mingw_dev_libs/SDL2/include/SDL2

# Specify SDL2 library directory
LIBRARY_PATHS = -LC:/sdl2_mingw_dev_libs/SDL2/lib

# Specify required libraries to resolve
LINKER_FLAGS = -lmingw32 -lSDL2main -lSDL2

# Targets
compile : $(OBJS)
	$(CC) $(OBJS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)

run:
	builds/driver

compileandrun: compile run
