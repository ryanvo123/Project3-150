# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99

# Source files
SRCS = disk.c fs.c

# Object files
OBJS = $(SRCS:.c=.o)

# Header files
HDRS = disk.h fs.h

# Library name
LIBRARY = libfs.a

# Build target
all: $(LIBRARY)

# Rule to compile object files
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build library
$(LIBRARY): $(OBJS)
	ar rcs $@ $(OBJS)

# Clean target
clean:
	rm -f $(OBJS) $(LIBRARY)