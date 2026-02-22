# compiler settings
CC = gcc
CFLAGS = -Wall -Iinclude
LDFLAGS = -lSDL2 -lSDL2_ttf -lssl -lcrypto

# directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
TARGET = browser

# default target
all: $(TARGET)

# link the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# compile c files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# clean up
clean:
	rm -rf $(BUILD_DIR) $(TARGET) temp_page.html
