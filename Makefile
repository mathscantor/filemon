# Variables
CC = gcc
CFLAGS = -Wall -Wextra -Wformat -Wformat-overflow -I./src -Iinclude -pthread
SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/filemon

# Source files
SRCS = $(wildcard $(SRC_DIR)/filemon.c $(SRC_DIR)/**/*.c)  # Includes .c files in subdirectories
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: shared

# Build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/logging
	mkdir -p $(BUILD_DIR)/helpers
	mkdir -p $(BUILD_DIR)/args
	mkdir -p $(BUILD_DIR)/monitoring


# Compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link the shared binary
shared: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# Link the static binary
static: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -static

# Clean up
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean shared static
