# Makefile for building the 'writer' application

# Default compiler (can be overridden by setting CROSS_COMPILE when calling make)
CC := gcc

# Check if the CROSS_COMPILE variable is set and adjust the compiler accordingly
ifdef CROSS_COMPILE
    CC := $(CROSS_COMPILE)gcc
    # Specify the path to your cross-compiler
    PATH := $(HOME)/arm-cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin:$(PATH)
endif

# Compiler flags
CFLAGS := -Wall -g

# Source and object files
SRC := writer.c
OBJ := $(SRC:.c=.o)

# Default target
all: writer

# Rule to build the writer application
writer: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f writer $(OBJ)

# Phony targets
.PHONY: all clean