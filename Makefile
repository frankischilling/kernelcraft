.DEFAULT_GOAL := all

ifeq ($(OS),Windows_NT)
WINDOWS_CMD := $(subst \,/,$(or $(ComSpec),$(COMSPEC),C:/Windows/System32/cmd.exe))
ifeq ($(filter /%,$(CURDIR)),)
SHELL := $(or $(ComSpec),$(COMSPEC),cmd.exe)
.SHELLFLAGS := /c
else
# MSYS Make runs recipes through sh; sending /c through its argument conversion
# starts an interactive cmd session instead of executing the build.
SHELL := /bin/sh
.SHELLFLAGS := -c
endif
CONFIGURATION ?= Release
WINDOWS_POWERSHELL = "$(dir $(WINDOWS_CMD))WindowsPowerShell/v1.0/powershell.exe"
WINDOWS_BUILD = $(WINDOWS_POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Configuration $(CONFIGURATION)

all copy_assets:
	$(WINDOWS_BUILD)
run:
	$(WINDOWS_BUILD) -Run
test test-gl:
	$(WINDOWS_BUILD) -Test
benchmark:
	$(WINDOWS_BUILD) -Benchmark
clean:
	$(WINDOWS_BUILD) -Clean
test-sanitize:
	@echo "MinGW GCC does not provide the sanitizer runtime. Use build.cmd -Configuration Debug -Test for native Windows checks."
	@$(WINDOWS_POWERSHELL) -NoProfile -Command "exit 1"

.PHONY: all clean run copy_assets test test-gl benchmark test-sanitize
else
CC = gcc
CFLAGS ?= -O2 -Wall
CPPFLAGS += -I./src
LDLIBS += -lGL -lglfw -lGLEW -lm -lglut
EXECUTABLE = $(BIN_DIR)/minecraft_clone
CREATE_BIN_DIR = @mkdir -p $(BIN_DIR)
CREATE_SUBDIR = @mkdir -p $(dir $@)
COPY_ASSET_DIR = @cp -r $(ASSET_DIR)/ $(BIN_DIR)/

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
ASSET_DIR = $(SRC_DIR)/assets
BIN_ASSET_DIR = $(BIN_DIR)/assets
LOG_FILE = build.log

SOURCES = $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
HEADERS = $(wildcard $(SRC_DIR)/*/*.h)
WORLD_TEST_SOURCES = tests/test_world.c $(wildcard $(SRC_DIR)/world/*.c) $(SRC_DIR)/math/math.c $(SRC_DIR)/graphics/frustum.c
SHADER_TEST_SOURCES = tests/test_shader.c $(SRC_DIR)/graphics/shader.c $(SRC_DIR)/graphics/texture.c
BENCHMARK_SOURCES = tests/render_benchmark.c $(filter-out $(SRC_DIR)/main.c,$(SOURCES))

all: $(EXECUTABLE) copy_assets

$(EXECUTABLE): $(OBJECTS)
	$(CREATE_BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "Build completed. Executable: $@"
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CREATE_SUBDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

copy_assets:
	$(CREATE_BIN_DIR)
	$(COPY_ASSET_DIR)
	@echo "Assets copied to: $(BIN_ASSET_DIR)"

run: $(EXECUTABLE) copy_assets
	@echo "Running $(EXECUTABLE)..."
	@cd $(BIN_DIR) && ./$(notdir $(EXECUTABLE))

clean:
	@rm -rf $(OBJ_DIR)
	@rm -rf $(BIN_DIR)
	@rm -f $(LOG_FILE)
	@echo "Clean completed."

$(BIN_DIR)/test-world: $(WORLD_TEST_SOURCES) $(HEADERS)
	$(CREATE_BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WORLD_TEST_SOURCES) -o $@ -lm

test: $(BIN_DIR)/test-world
	./$(BIN_DIR)/test-world

$(BIN_DIR)/test-shader: $(SHADER_TEST_SOURCES) $(HEADERS)
	$(CREATE_BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SHADER_TEST_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN_DIR)/benchmark: $(BENCHMARK_SOURCES) $(HEADERS)
	$(CREATE_BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BENCHMARK_SOURCES) -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements -o $@ $(LDFLAGS) $(LDLIBS)

test-gl: $(BIN_DIR)/test-shader $(BIN_DIR)/benchmark copy_assets
	xvfb-run -a ./$(BIN_DIR)/test-shader
	cd $(BIN_DIR) && xvfb-run -a ./benchmark

benchmark: $(BIN_DIR)/benchmark copy_assets
	cd $(BIN_DIR) && xvfb-run -a ./benchmark

test-sanitize:
	$(CREATE_BIN_DIR)
	$(CC) $(CPPFLAGS) -O1 -g -Wall -fsanitize=address,undefined $(WORLD_TEST_SOURCES) -o $(BIN_DIR)/test-world-sanitize -lm
	./$(BIN_DIR)/test-world-sanitize

.PHONY: all clean run copy_assets test test-gl benchmark test-sanitize
endif
