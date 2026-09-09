#ifndef SAVE_H
#define SAVE_H

#include "../math/math.h"
#include <stddef.h>

typedef struct {
  Vec3 feet;
  float yaw, pitch;
  int selectedSlot; // Zero-based hotbar slot, including empty slots.
} SavedPlayer;

typedef enum { SAVE_OK, SAVE_NOT_FOUND, SAVE_INVALID, SAVE_UNSUPPORTED, SAVE_IO_ERROR, SAVE_NO_MEMORY } SaveResult;

// Paths use the platform C runtime encoding. Success clears error, if provided.
SaveResult saveWorld(const char* path, const SavedPlayer* player, char* error, size_t capacity);
// Failure leaves live chunks, seed, and output player unchanged.
SaveResult loadWorld(const char* path, SavedPlayer* player, char* error, size_t capacity);

#endif
