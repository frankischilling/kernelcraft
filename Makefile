CC = gcc
CFLAGS = -Wall -I./src
LDFLAGS = -lGL -lglfw -lGLEW -lm -lglut

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_FILE = build.log

SOURCES = $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = $(BIN_DIR)/minecraft_clone

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) > $(LOG_FILE) 2>&1

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ >> $(LOG_FILE) 2>&1

run: $(EXECUTABLE)
	./$(EXECUTABLE)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LOG_FILE)

.PHONY: all clean run
