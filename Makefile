# Variables
CC := gcc
ARCH ?= x86_64  # Default architecture
CFLAGS := -Wall -Wextra -Wformat -Wformat-overflow -I./src -Iinclude -pthread
LDFLAGS := -lz
SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/filemon

# Set compiler and flags based on architecture
ARCH := $(strip $(ARCH))
ifeq ($(ARCH),x86_64)
    CC := gcc
else ifeq ($(ARCH),x86)
    CC := gcc
    CFLAGS += -m32
else ifeq ($(ARCH),aarch64)
    CC := aarch64-linux-gnu-gcc
else ifeq ($(ARCH),arm)
    CC := arm-linux-gnueabihf-gcc
else
    $(error Unsupported ARCH: $(ARCH). Use ARCH=x86_64, ARCH=x86, ARCH=aarch64, or ARCH=arm)
endif

# Check if the compiler is installed
ifeq ($(shell command -v $(CC) 2>/dev/null),)
    $(error Compiler $(CC) not found! Please install it before proceeding.)
endif

# Source files
SRCS := $(wildcard $(SRC_DIR)/filemon.c $(SRC_DIR)/**/*.c)  # Includes .c files in subdirectories
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: shared

# Build directory
SUBDIRS := logging helpers args monitoring
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(foreach dir, $(SUBDIRS), $(BUILD_DIR)/$(dir))

# Compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link the shared binary
shared: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Link the static binary
static: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -static $(LDFLAGS)

# Clean up
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean shared static
