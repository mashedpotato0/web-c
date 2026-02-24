CC = gcc
# add external/clay to include path
CFLAGS = -Wall -Iinclude -Iexternal/clay -I.
LDFLAGS = -lSDL2 -lSDL2_ttf -lSDL2_image -lssl -lcrypto -lduktape -lm

SRC_DIR = src
BUILD_DIR = build
TARGET = browser

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# init submodule instead of downloading
setup:
	git submodule update --init
	@echo "clay submodule ready"

clean:
	rm -rf $(BUILD_DIR) $(TARGET) temp_page.html temp_assets
