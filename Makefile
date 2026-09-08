CC = gcc
CFLAGS ?= -O2 -Wall
CPPFLAGS += -I./src
.DEFAULT_GOAL := all

ifeq ($(OS),Windows_NT)
	LIBRARY_DIR ?= C:/Progs/vcpkg/installed/x64-windows

	CPPFLAGS += -I"$(LIBRARY_DIR)/include"
	LDFLAGS += -L"$(LIBRARY_DIR)/lib"
	LDLIBS += -lopengl32 -lglfw3dll -lglew32 -lm -lfreeglut

	EXECUTABLE = $(BIN_DIR)/minecraft_clone.exe

	CREATE_BIN_DIR = @if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	CREATE_SUBDIR = @if not exist "$(dir $@)" mkdir "$(dir $@)"

	COPY_ASSET_DIR = @xcopy "$(SRC_DIR)\assets" "$(BIN_DIR)\assets\" /E /I /Q

	DLLS_TO_COPY = freeglut.dll glew32.dll glfw3.dll
else
	LDLIBS += -lGL -lglfw -lGLEW -lm -lglut

	EXECUTABLE = $(BIN_DIR)/minecraft_clone

	CREATE_BIN_DIR = @mkdir -p $(BIN_DIR)
	CREATE_SUBDIR = @mkdir -p $(dir $@)

	COPY_ASSET_DIR = @cp -r $(ASSET_DIR)/ $(BIN_DIR)/

endif

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
ifeq ($(OS),Windows_NT)
	@for %%i in ($(DLLS_TO_COPY)) do copy /Y "$(LIBRARY_DIR)\bin\%%i" "$(BIN_DIR)\"
endif
	@echo "Assets copied to: $(BIN_ASSET_DIR)"

run: $(EXECUTABLE) copy_assets
	@echo "Running $(EXECUTABLE)..."
ifeq ($(OS),Windows_NT)
	@cd $(BIN_DIR) && $(notdir $(EXECUTABLE))
else
	@cd $(BIN_DIR) && ./$(notdir $(EXECUTABLE))
endif

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)
	@if exist $(LOG_FILE) del /q $(LOG_FILE)
else
	@rm -rf $(OBJ_DIR)
	@rm -rf $(BIN_DIR)
	@rm -f $(LOG_FILE)
endif
	@echo "Clean completed."

$(BIN_DIR)/test-world: $(WORLD_TEST_SOURCES) $(HEADERS)
	$(CREATE_BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WORLD_TEST_SOURCES) -o $@ -lm

test: $(BIN_DIR)/test-world
	./$(BIN_DIR)/test-world

ifeq ($(OS),Windows_NT)
test-gl benchmark test-sanitize:
	@echo "Run this check from Linux or WSL; see docs/performance.md."
	@exit 1
else
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
endif

.PHONY: all clean run copy_assets test test-gl benchmark test-sanitize
