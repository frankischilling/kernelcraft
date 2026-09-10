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
test-build:
	$(WINDOWS_POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File tests/test_build.ps1
benchmark:
	$(WINDOWS_BUILD) -Benchmark
clean:
	$(WINDOWS_BUILD) -Clean
test-sanitize:
	@echo "MinGW GCC does not provide the sanitizer runtime. Use build.cmd -Configuration Debug -Test for native Windows checks."
	@$(WINDOWS_POWERSHELL) -NoProfile -Command "exit 1"

.PHONY: all clean run copy_assets test test-gl test-build benchmark test-sanitize
else
# Respect CC from the environment as well as command-line overrides.
ifeq ($(origin CC),default)
CC := gcc
endif
CONFIGURATION ?= Release
ifneq ($(CONFIGURATION),Release)
ifneq ($(CONFIGURATION),Debug)
$(error CONFIGURATION must be Release or Debug)
endif
endif
ifeq ($(CONFIGURATION),Debug)
CFLAGS ?= -O0 -g3
else
CFLAGS ?= -O2 -g
endif
COMPILE_FLAGS = -std=c11 -Wall -Wformat=2 -Wstrict-prototypes $(CFLAGS)
PKG_CONFIG ?= pkg-config
GRAPHICS_PACKAGES ?= gl glfw3 glew glut
GRAPHICS_CPPFLAGS ?= $(shell $(PKG_CONFIG) --cflags $(GRAPHICS_PACKAGES) 2>/dev/null)
GRAPHICS_LDLIBS ?= $(shell $(PKG_CONFIG) --libs $(GRAPHICS_PACKAGES) 2>/dev/null)
PROJECT_CPPFLAGS = -Isrc $(GRAPHICS_CPPFLAGS) $(CPPFLAGS)
PROJECT_LDLIBS = $(GRAPHICS_LDLIBS) -lm $(LDLIBS)
OBJ_DIR := obj/linux/$(CONFIGURATION)
BIN_DIR := bin/linux/$(CONFIGURATION)
EXECUTABLE := $(BIN_DIR)/minecraft_clone
BUILD_SETTINGS := $(OBJ_DIR)/build-settings
SOURCES := $(wildcard src/*/*.c src/*.c)
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))
WORLD_SOURCES := $(wildcard src/world/*.c) src/math/math.c src/graphics/frustum.c src/utils/raycast.c
WORLD_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(WORLD_SOURCES))
SHADER_OBJECTS := $(OBJ_DIR)/src/graphics/shader.o $(OBJ_DIR)/src/graphics/texture.o
TEST_SOURCES := tests/test_options.c tests/test_save.c tests/test_seed.c tests/test_player.c tests/test_selection.c tests/test_edits.c tests/test_world.c tests/test_shader.c tests/render_benchmark.c tests/app_smoke.c tests/app_persistence.c
TEST_SOURCES += tests/test_hud.c
TEST_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SOURCES))
WRAP_STARTUP := -Wl,--wrap=glfwCreateWindow -Wl,--wrap=glfwWindowShouldClose -Wl,--wrap=glfwSetInputMode -Wl,--wrap=glfwDestroyWindow -Wl,--wrap=glfwGetInputMode -Wl,--wrap=glfwGetWindowAttrib -Wl,--wrap=glfwGetKey -Wl,--wrap=glfwGetFramebufferSize -Wl,--wrap=glfwWaitEvents -Wl,--wrap=glfwSwapBuffers -Wl,--wrap=glfwGetTime -Wl,--wrap=HUDDraw
WRAP_PERSISTENCE := $(filter-out %--wrap=glfwGetFramebufferSize %--wrap=glfwWaitEvents,$(WRAP_STARTUP))
WRAP_BENCHMARK := -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements -Wl,--wrap=occlusionBoundsHidden -Wl,--wrap=meshVisibilityIntersects

# Quote option text as data, including embedded single quotes. Keep this in a
# recipe so make -n never writes files while expanding the build settings.
shell_quote = '$(subst ','"'"',$(1))'

all: $(EXECUTABLE) copy_assets

check-deps:
	@if ! command -v $(firstword $(CC)) >/dev/null 2>&1; then \
	  echo "C compiler not found: $(firstword $(CC))" >&2; exit 1; fi
	@if [ -z "$(strip $(GRAPHICS_LDLIBS))" ]; then \
	  echo "Graphics dependencies not found. Install pkg-config and OpenGL/GLFW/GLEW/freeglut development packages (see README.md), or set GRAPHICS_CPPFLAGS and GRAPHICS_LDLIBS." >&2; exit 1; fi

$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

$(BUILD_SETTINGS): FORCE | $(OBJ_DIR)
	@printf '%s\n' $(call shell_quote,CC=$(CC)) $(call shell_quote,CPPFLAGS=$(PROJECT_CPPFLAGS)) $(call shell_quote,CFLAGS=$(COMPILE_FLAGS)) $(call shell_quote,LDFLAGS=$(LDFLAGS)) $(call shell_quote,LDLIBS=$(PROJECT_LDLIBS)) >$@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

$(OBJ_DIR)/%.o: %.c $(BUILD_SETTINGS) | check-deps
	@mkdir -p $(dir $@)
	$(CC) $(PROJECT_CPPFLAGS) $(COMPILE_FLAGS) -MMD -MP -c $< -o $@

# CPU tests and their shared objects need neither GL headers nor graphics packages.
$(OBJ_DIR)/src/utils/options.o $(OBJ_DIR)/tests/test_options.o $(WORLD_OBJECTS) $(OBJ_DIR)/tests/test_world.o $(OBJ_DIR)/tests/test_edits.o $(OBJ_DIR)/tests/test_selection.o $(OBJ_DIR)/tests/test_player.o $(OBJ_DIR)/tests/test_seed.o $(OBJ_DIR)/tests/test_save.o: $(OBJ_DIR)/%.o: %.c $(BUILD_SETTINGS)
	@mkdir -p $(dir $@)
	$(CC) -Isrc $(CPPFLAGS) $(COMPILE_FLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

$(EXECUTABLE): $(OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(PROJECT_LDLIBS)

copy_assets: | $(BIN_DIR)
	@cp -R src/assets $(BIN_DIR)/

run: all
	./$(EXECUTABLE)

# Linux clean leaves native Windows artifacts and the other configuration intact.
clean:
	rm -rf -- $(OBJ_DIR) $(BIN_DIR)

$(BIN_DIR)/test-world: $(OBJ_DIR)/tests/test_world.o $(WORLD_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) -lm $(LDLIBS)

$(BIN_DIR)/test-edits: $(OBJ_DIR)/tests/test_edits.o $(WORLD_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) -lm $(LDLIBS)

$(BIN_DIR)/test-selection: $(OBJ_DIR)/tests/test_selection.o $(WORLD_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) -lm $(LDLIBS)

$(BIN_DIR)/test-player: $(OBJ_DIR)/tests/test_player.o $(WORLD_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) -lm $(LDLIBS)

$(BIN_DIR)/test-seed: $(OBJ_DIR)/tests/test_seed.o $(WORLD_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) -lm $(LDLIBS)

$(BIN_DIR)/test-save: $(OBJ_DIR)/tests/test_save.o $(WORLD_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -Wl,--wrap=fwrite -Wl,--wrap=fflush -Wl,--wrap=fclose -Wl,--wrap=fsync -Wl,--wrap=rename -Wl,--wrap=calloc -o $@ $(LDFLAGS) -lm $(LDLIBS)

$(BIN_DIR)/test-options: $(OBJ_DIR)/tests/test_options.o $(OBJ_DIR)/src/utils/options.o $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) $(LDLIBS)

test: $(BIN_DIR)/test-options $(BIN_DIR)/test-world $(BIN_DIR)/test-edits $(BIN_DIR)/test-selection $(BIN_DIR)/test-player $(BIN_DIR)/test-seed $(BIN_DIR)/test-save
	./$(BIN_DIR)/test-options
	./$(BIN_DIR)/test-world
	./$(BIN_DIR)/test-edits
	./$(BIN_DIR)/test-selection
	./$(BIN_DIR)/test-player
	./$(BIN_DIR)/test-seed
	./$(BIN_DIR)/test-save

$(BIN_DIR)/test-shader: $(OBJ_DIR)/tests/test_shader.o $(SHADER_OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -o $@ $(LDFLAGS) $(PROJECT_LDLIBS)

$(BIN_DIR)/benchmark: $(OBJ_DIR)/tests/render_benchmark.o $(filter-out $(OBJ_DIR)/src/main.o,$(OBJECTS)) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) $(WRAP_BENCHMARK) -o $@ $(LDFLAGS) $(PROJECT_LDLIBS)

$(BIN_DIR)/test-hud: $(OBJ_DIR)/tests/test_hud.o $(filter-out $(OBJ_DIR)/src/main.o,$(OBJECTS)) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) -Wl,--wrap=renderText -Wl,--wrap=loadTexture -o $@ $(LDFLAGS) $(PROJECT_LDLIBS)

$(BIN_DIR)/test-startup: $(OBJ_DIR)/tests/app_smoke.o $(OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) $(WRAP_STARTUP) -o $@ $(LDFLAGS) $(PROJECT_LDLIBS)

$(BIN_DIR)/test-persistence: $(OBJ_DIR)/tests/app_persistence.o $(OBJECTS) $(BUILD_SETTINGS) | $(BIN_DIR)
	$(CC) $(filter %.o,$^) $(WRAP_PERSISTENCE) -o $@ $(LDFLAGS) $(PROJECT_LDLIBS)

test-gl: $(BIN_DIR)/test-hud $(BIN_DIR)/test-persistence $(BIN_DIR)/test-shader $(BIN_DIR)/benchmark $(BIN_DIR)/test-startup copy_assets
	cd $(BIN_DIR) && xvfb-run -a ./test-hud
	cd $(BIN_DIR) && xvfb-run -a ./test-shader
	cd $(BIN_DIR) && xvfb-run -a ./benchmark
	xvfb-run -a sh tests/test_startup.sh $(BIN_DIR)/test-startup
	xvfb-run -a sh tests/test_persistence.sh $(BIN_DIR)/test-persistence

benchmark: $(BIN_DIR)/benchmark copy_assets
	cd $(BIN_DIR) && xvfb-run -a ./benchmark

test-sanitize:
	$(MAKE) CONFIGURATION=Debug CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='$(LDFLAGS) -fsanitize=address,undefined' test

test-build:
	sh tests/test_build.sh

FORCE:
.PHONY: all check-deps clean run copy_assets test test-gl benchmark test-sanitize test-build FORCE
endif
