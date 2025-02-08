# Target, source, and object files
TARGET = aesdsocket
SRCS = aesdsocket.c
OBJS = aesdsocket.o

# Compiler and flags
CC ?= gcc
CFLAGS = -g -Wall -Werror

# If Cross Compiler provided as argument
ifdef CROSS_COMPILE
	CC := $(CROSS_COMPILE)gcc
endif

# Default and all targets should both compile the target
default: $(TARGET)

all: $(TARGET)

# First compile source to object file, then link to create executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)
	chmod +x $(TARGET)

# Compile source files into object files
$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -c $(SRCS) -o $(OBJS)

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Declare phony targets to prevent conflicts with file names
.PHONY: all clean default
