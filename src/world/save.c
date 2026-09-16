#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "save.h"
#include "world.h"
#include "player.h"
#include "hotbar.h"
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#define SAVE_VERSION 5
#define HEADER_BYTES 72
#define EXTENSION_HEADER_BYTES 16
#define STACK_BYTES 8
#define DROP_BYTES 20
#define EXTENSION_FIXED_BYTES (EXTENSION_HEADER_BYTES + INVENTORY_SERIALIZED_STACK_COUNT * STACK_BYTES)
#define EXTENSION_MAX_BYTES (EXTENSION_FIXED_BYTES + DROPPED_ITEM_CAPACITY * DROP_BYTES)
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128, "Save format requires IEEE binary32 floats");
_Static_assert(INVENTORY_CARRIED_SLOT_COUNT == 36 && INVENTORY_ARMOR_SLOT_COUNT == 4 && INVENTORY_CRAFTING_SLOT_COUNT == 4 && INVENTORY_SERIALIZED_STACK_COUNT == 46,
               "Save version 5 inventory layout changed");
_Static_assert(DROPPED_ITEM_CAPACITY == 128, "Save version 5 dropped-item capacity changed");

static SaveResult result(SaveResult code, char* error, size_t capacity, const char* message) {
  if (error && capacity)
    snprintf(error, capacity, "%s", message);
  return code;
}

static uint32_t get32(const uint8_t* bytes) {
  return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static void put32(uint8_t* bytes, uint32_t value) {
  for (int i = 0; i < 4; i++)
    bytes[i] = (uint8_t)(value >> (8 * i));
}

static float getFloat(const uint8_t* bytes) {
  uint32_t bits = get32(bytes);
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static void putFloat(uint8_t* bytes, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  put32(bytes, bits);
}

static uint32_t checksum(const uint8_t* header, const uint8_t* blocks, const uint8_t* extension, size_t extensionBytes) {
  uint32_t hash = UINT32_C(2166136261);
  for (size_t i = 0; i < 68; i++)
    hash = (hash ^ header[i]) * UINT32_C(16777619);
  for (size_t i = 0; i < WORLD_BLOCK_COUNT; i++)
    hash = (hash ^ blocks[i]) * UINT32_C(16777619);
  for (size_t i = 0; i < extensionBytes; i++)
    hash = (hash ^ extension[i]) * UINT32_C(16777619);
  return hash;
}

static bool validPlayer(const SavedPlayer* player, const uint8_t* blocks) {
  Vec3i first, last;
  if (!player || !isfinite(player->yaw) || player->yaw < 0 || player->yaw >= 360 || !isfinite(player->pitch) || player->pitch < -89 || player->pitch > 89 ||
      player->selectedSlot < 0 || player->selectedSlot >= HOTBAR_SLOT_COUNT || !inventoryValidate(&player->inventory) || !droppedItemsValid(&player->drops) ||
      !playerCellRange(player->feet, &first, &last))
    return false;
  for (int x = first.x; x <= last.x; x++)
    for (int y = first.y; y <= last.y; y++)
      for (int z = first.z; z <= last.z; z++) {
        int wx = x + WORLD_SIZE / 2, wz = z + WORLD_SIZE / 2;
        size_t chunk = (size_t)(wx / CHUNK_SIZE) * CHUNKS_PER_AXIS + wz / CHUNK_SIZE;
        size_t offset = ((chunk * CHUNK_SIZE + wx % CHUNK_SIZE) * CHUNK_HEIGHT + y) * CHUNK_SIZE + wz % CHUNK_SIZE;
        if (blockIsSolid(blocks[offset]))
          return false;
      }

  return true;
}

static void putStack(uint8_t* bytes, ItemStack stack) {
  put32(bytes, stack.item);
  put32(bytes + 4, stack.count);
}

static bool getStack(const uint8_t* bytes, ItemStack* stack) {
  uint32_t item = get32(bytes), count = get32(bytes + 4);
  if (item > UINT16_MAX || count > UINT16_MAX)
    return false;
  *stack = (ItemStack){.item = (uint16_t)item, .count = (uint16_t)count};
  return true;
}

static bool encodeExtension(const SavedPlayer* player, uint8_t* extension, size_t* extensionBytes) {
  ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT];
  if (!inventoryExportStacks(&player->inventory, stacks) || !droppedItemsValid(&player->drops))
    return false;

  uint32_t dropCount = 0;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    dropCount += player->drops.items[i].active ? 1u : 0u;

  put32(extension, INVENTORY_CARRIED_SLOT_COUNT);
  put32(extension + 4, INVENTORY_ARMOR_SLOT_COUNT);
  put32(extension + 8, INVENTORY_CRAFTING_SLOT_COUNT);
  put32(extension + 12, dropCount);
  size_t offset = EXTENSION_HEADER_BYTES;
  for (size_t i = 0; i < INVENTORY_SERIALIZED_STACK_COUNT; i++, offset += STACK_BYTES)
    putStack(extension + offset, stacks[i]);

  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++) {
    const DroppedItem* drop = &player->drops.items[i];
    if (!drop->active)
      continue;
    putFloat(extension + offset, drop->position.x);
    putFloat(extension + offset + 4, drop->position.y);
    putFloat(extension + offset + 8, drop->position.z);
    putStack(extension + offset + 12, drop->stack);
    offset += DROP_BYTES;
  }

  *extensionBytes = offset;
  return offset <= EXTENSION_MAX_BYTES;
}

static bool decodeExtension(const uint8_t* extension, size_t extensionBytes, Inventory* inventory, DroppedItems* drops) {
  if (extensionBytes < EXTENSION_FIXED_BYTES || get32(extension) != INVENTORY_CARRIED_SLOT_COUNT || get32(extension + 4) != INVENTORY_ARMOR_SLOT_COUNT ||
      get32(extension + 8) != INVENTORY_CRAFTING_SLOT_COUNT)
    return false;
  uint32_t dropCount = get32(extension + 12);
  if (dropCount > DROPPED_ITEM_CAPACITY || extensionBytes != EXTENSION_FIXED_BYTES + (size_t)dropCount * DROP_BYTES)
    return false;

  ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT];
  size_t offset = EXTENSION_HEADER_BYTES;
  for (size_t i = 0; i < INVENTORY_SERIALIZED_STACK_COUNT; i++, offset += STACK_BYTES)
    if (!getStack(extension + offset, &stacks[i]))
      return false;
  if (!inventoryImportStacks(inventory, stacks))
    return false;

  *drops = (DroppedItems){0};
  for (uint32_t i = 0; i < dropCount; i++) {
    DroppedItem* drop = &drops->items[i];
    drop->position = (Vec3){getFloat(extension + offset), getFloat(extension + offset + 4), getFloat(extension + offset + 8)};
    if (!getStack(extension + offset + 12, &drop->stack))
      return false;
    drop->pickupDelay = 0.5f;
    drop->active = true;
    offset += DROP_BYTES;
  }
  return droppedItemsValid(drops);
}

static FILE* createTemporary(const char* path, char* temporary, size_t capacity) {
  static unsigned sequence;
#ifdef _WIN32
  unsigned long pid = (unsigned long)_getpid();
#else
  unsigned long pid = (unsigned long)getpid();
#endif
  for (int attempt = 0; attempt < 128; attempt++) {
    snprintf(temporary, capacity, "%s.tmp-%lu-%u", path, pid, sequence++);
#ifdef _WIN32
    int descriptor = _open(temporary, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    int descriptor = open(temporary, O_CREAT | O_EXCL | O_WRONLY, 0600);
#endif
    if (descriptor < 0) {
      if (errno == EEXIST)
        continue;
      return NULL;
    }
#ifdef _WIN32
    FILE* file = _fdopen(descriptor, "wb");
#else
    FILE* file = fdopen(descriptor, "wb");
#endif
    if (!file) {
      int savedError = errno;
#ifdef _WIN32
      _close(descriptor);
#else
      close(descriptor);
#endif
      remove(temporary);
      errno = savedError;
    }

    return file;
  }

  errno = EEXIST;
  return NULL;
}

static int syncFile(FILE* file) {
#ifdef _WIN32
  return _commit(_fileno(file));
#else
  return fsync(fileno(file));
#endif
}

SaveResult saveWorld(const char* path, const SavedPlayer* player, char* error, size_t capacity) {
  if (!path || !path[0] || strlen(path) > 4095 || !player)
    return result(SAVE_INVALID, error, capacity, "Invalid save path or player");
  uint8_t* blocks = malloc(WORLD_BLOCK_COUNT);
  if (!blocks)
    return result(SAVE_NO_MEMORY, error, capacity, "Not enough memory to save world");
  if (!copyWorldBlocks(blocks, WORLD_BLOCK_COUNT) || !validPlayer(player, blocks)) {
    free(blocks);
    return result(SAVE_INVALID, error, capacity, "World or saved player is invalid");
  }

  uint8_t extension[EXTENSION_MAX_BYTES] = {0};
  size_t extensionBytes;
  if (!encodeExtension(player, extension, &extensionBytes)) {
    free(blocks);
    return result(SAVE_INVALID, error, capacity, "Inventory or dropped-item state is invalid");
  }

  uint8_t header[HEADER_BYTES] = {0};
  memcpy(header, "KCRFTSV\0", 8);
  put32(header + 8, SAVE_VERSION);
  put32(header + 12, WORLD_GENERATOR_VERSION);
  put32(header + 16, worldSeed());
  put32(header + 20, WORLD_SIZE);
  put32(header + 24, CHUNK_HEIGHT);
  put32(header + 28, CHUNK_SIZE);
  put32(header + 32, CHUNKS_PER_AXIS * CHUNKS_PER_AXIS);
  put32(header + 36, (uint32_t)WORLD_BLOCK_COUNT);
  putFloat(header + 40, player->feet.x);
  putFloat(header + 44, player->feet.y);
  putFloat(header + 48, player->feet.z);
  putFloat(header + 52, player->yaw);
  putFloat(header + 56, player->pitch);
  put32(header + 60, (uint32_t)player->selectedSlot + 1);
  put32(header + 64, (uint32_t)extensionBytes);
  put32(header + 68, checksum(header, blocks, extension, extensionBytes));
  char temporary[4160];
  FILE* file = createTemporary(path, temporary, sizeof(temporary));
  if (!file) {
    free(blocks);
    return result(SAVE_IO_ERROR, error, capacity, "Cannot create a temporary save beside the destination");
  }

  bool written = fwrite(header, 1, sizeof(header), file) == sizeof(header) && fwrite(blocks, 1, WORLD_BLOCK_COUNT, file) == WORLD_BLOCK_COUNT &&
                 fwrite(extension, 1, extensionBytes, file) == extensionBytes;
  if (written)
    written = fflush(file) == 0 && syncFile(file) == 0;
  if (fclose(file) != 0)
    written = false;
  free(blocks);
  if (!written) {
    bool removed = remove(temporary) == 0;
    return result(SAVE_IO_ERROR, error, capacity,
                  removed ? "Save write, flush, sync, or close failed; previous save retained" : "Save failed; temporary file could not be removed");
  }
#ifdef _WIN32
  bool replaced = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  bool replaced = rename(temporary, path) == 0;
#endif
  if (!replaced) {
    bool removed = remove(temporary) == 0;
    return result(SAVE_IO_ERROR, error, capacity, removed ? "Cannot replace destination; previous save retained" : "Cannot replace destination or remove temporary save");
  }

  return result(SAVE_OK, error, capacity, "");
}

SaveResult loadWorld(const char* path, SavedPlayer* player, char* error, size_t capacity) {
  if (!path || !path[0] || strlen(path) > 4095 || !player)
    return result(SAVE_INVALID, error, capacity, "Invalid save path or player output");
  FILE* file = fopen(path, "rb");
  if (!file)
    return result(errno == ENOENT ? SAVE_NOT_FOUND : SAVE_IO_ERROR, error, capacity, "Cannot open world save");
  uint8_t header[HEADER_BYTES];
  if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
    bool failed = ferror(file) != 0;
    fclose(file);
    return result(failed ? SAVE_IO_ERROR : SAVE_INVALID, error, capacity, "Save header is unreadable or truncated");
  }

  if (memcmp(header, "KCRFTSV\0", 8) != 0) {
    fclose(file);
    return result(SAVE_INVALID, error, capacity, "Unrecognized world save header");
  }

  uint32_t version = get32(header + 8);
  if ((version < 1 || version > SAVE_VERSION) || get32(header + 12) != WORLD_GENERATOR_VERSION) {
    fclose(file);
    return result(SAVE_UNSUPPORTED, error, capacity, "Unsupported save or generator version");
  }

  if (get32(header + 20) != WORLD_SIZE || get32(header + 24) != CHUNK_HEIGHT || get32(header + 28) != CHUNK_SIZE || get32(header + 32) != CHUNKS_PER_AXIS * CHUNKS_PER_AXIS ||
      get32(header + 36) != WORLD_BLOCK_COUNT) {
    fclose(file);
    return result(SAVE_INVALID, error, capacity, "Invalid save dimensions or counts");
  }
  uint32_t extensionBytes = get32(header + 64);
  if ((version < 5 && extensionBytes != 0) || (version == 5 && (extensionBytes < EXTENSION_FIXED_BYTES || extensionBytes > EXTENSION_MAX_BYTES))) {
    fclose(file);
    return result(SAVE_INVALID, error, capacity, "Invalid save extension size");
  }

  uint8_t* blocks = malloc(WORLD_BLOCK_COUNT);
  if (!blocks) {
    fclose(file);
    return result(SAVE_NO_MEMORY, error, capacity, "Not enough memory to read world save");
  }

  uint8_t extension[EXTENSION_MAX_BYTES] = {0};
  size_t count = fread(blocks, 1, WORLD_BLOCK_COUNT, file);
  size_t extensionCount = count == WORLD_BLOCK_COUNT && extensionBytes ? fread(extension, 1, extensionBytes, file) : 0;
  int trailing = fgetc(file);
  bool readError = ferror(file) != 0;
  if (fclose(file) != 0)
    readError = true;
  if (readError || count != WORLD_BLOCK_COUNT || extensionCount != extensionBytes || trailing != EOF) {
    free(blocks);
    return result(readError ? SAVE_IO_ERROR : SAVE_INVALID, error, capacity, "Save payload is unreadable, truncated, or has trailing data");
  }

  bool valid = get32(header + 68) == checksum(header, blocks, extension, extensionBytes);
  int lastBlock = version < 3 ? BLOCK_STONE : version == 3 ? BLOCK_COBBLESTONE : BLOCK_STONE_BRICKS;
  for (size_t i = 0; valid && i < WORLD_BLOCK_COUNT; i++)
    valid = blockIDValid(blocks[i]) && blocks[i] <= lastBlock;
  uint32_t selected = get32(header + 60);
  // Version 1 stored block IDs 1..3, matching the first three numbered slots.
  uint32_t lastSlot = version == 1 ? 3 : HOTBAR_SLOT_COUNT;
  SavedPlayer loaded = {.feet = {getFloat(header + 40), getFloat(header + 44), getFloat(header + 48)},
                        .yaw = getFloat(header + 52),
                        .pitch = getFloat(header + 56),
                        .selectedSlot = selected >= 1 && selected <= lastSlot ? (int)selected - 1 : -1};
  if (version < 5) {
    inventoryInit(&loaded.inventory);
    loaded.drops = (DroppedItems){0};
  } else if (!decodeExtension(extension, extensionBytes, &loaded.inventory, &loaded.drops)) {
    valid = false;
  }
  if (!valid || !validPlayer(&loaded, blocks)) {
    free(blocks);
    return result(SAVE_INVALID, error, capacity, "Save checksum, block IDs, player, inventory, or dropped-item state is invalid");
  }

  bool installed = replaceWorldBlocks(get32(header + 16), blocks, WORLD_BLOCK_COUNT);
  free(blocks);
  if (!installed)
    return result(SAVE_NO_MEMORY, error, capacity, "Not enough memory to install world; current world retained");
  *player = loaded;
  return result(SAVE_OK, error, capacity, "");
}
