# Kernelcraft (rewrite) - minimal Minecraft-like sandbox (C99 + OpenGL + X11/GLX)
# Build deps (typical): mesa (libGL), libX11, headers for both.
#
# Directory layout:
#   src/   -> engine/game sources
#   libs/  -> third party headers (stb_image.h)
#   textures/ -> runtime assets (NOT built; expected in your repo)
#
# Usage:
#   make            (release-ish build)
#   make debug      (debug build)
#   make run        (build then run)
#   make clean

CC      ?= gcc
AR      ?= ar
RM      ?= rm -f

TARGET  := kernelcraft
BIN_DIR := bin
OBJ_DIR := build

SRCS := \
  src/main.c \
  src/log.c \
  src/platform_x11.c \
  src/gl_loader.c \
  src/math.c \
  src/camera.c \
  src/texture.c \
  src/atlas.c \
  src/mesh_builder.c \
  src/world.c \
  src/chunk_manager.c \
  src/renderer.c

OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INCLUDES := -Isrc -Ilibs

CFLAGS_COMMON := -std=c99 -D_POSIX_C_SOURCE=200809L \
  -Wall -Wextra -Wpedantic \
  -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
  -Wconversion -Wsign-conversion \
  -fno-common

# NOTE: -Wconversion can be noisy. If it becomes annoying during iteration,
# you can drop it, but it's useful early to keep the code tight.

CFLAGS_RELEASE := -O2 -DNDEBUG
CFLAGS_DEBUG   := -O0 -g3 -DDEBUG

LDFLAGS := -lX11 -lGL -lm

# Default: release-ish
CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_RELEASE)

.PHONY: all debug release clean run

all: release

release: CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_RELEASE)
release: $(BIN_DIR)/$(TARGET)

debug: CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_DEBUG)
debug: $(BIN_DIR)/$(TARGET)

run: $(BIN_DIR)/$(TARGET)
	@./$(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile rules (with dependency generation)
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR) $(BIN_DIR)

-include $(DEPS)
