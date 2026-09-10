#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "world/save.h"
#include "world/world.h"
#include "world/player.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
#define makeDirectory(p) _mkdir(p)
#define removeDirectory(p) _rmdir(p)
#define processID() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define makeDirectory(p) mkdir(p, 0700)
#define removeDirectory(p) rmdir(p)
#define processID() getpid()
#endif

#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "Save test: %s (line %d)\n", #c, __LINE__);                                                                                                                  \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)
static int failIO, failAllocation;
void* __real_calloc(size_t count, size_t size);

void* __wrap_calloc(size_t count, size_t size) {
  if (failAllocation && --failAllocation == 0) {
    errno = ENOMEM;
    return NULL;
  }

  return __real_calloc(count, size);
}
#ifdef _WIN32
extern int (*__real___imp__commit)(int descriptor);

int __wrap__commit(int descriptor) {
  if (failIO == 4) {
    failIO = 0;
    errno = EIO;
    return -1;
  }

  return __real___imp__commit(descriptor);
}

// MinGW calls _commit through its imported function pointer.
int (*__wrap___imp__commit)(int descriptor) = __wrap__commit;
extern BOOL(WINAPI* __real___imp_MoveFileExA)(LPCSTR, LPCSTR, DWORD);

static BOOL WINAPI replaceFile(LPCSTR from, LPCSTR to, DWORD flags) {
  if (failIO == 5) {
    failIO = 0;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }

  return __real___imp_MoveFileExA(from, to, flags);
}

BOOL(WINAPI* __wrap___imp_MoveFileExA)(LPCSTR, LPCSTR, DWORD) = replaceFile;
#else
int __real_rename(const char* from, const char* to);

int __wrap_rename(const char* from, const char* to) {
  if (failIO == 5) {
    failIO = 0;
    errno = EACCES;
    return -1;
  }

  return __real_rename(from, to);
}

int __real_fsync(int descriptor);

int __wrap_fsync(int descriptor) {
  if (failIO == 4) {
    failIO = 0;
    errno = EIO;
    return -1;
  }

  return __real_fsync(descriptor);
}
#endif
size_t __real_fwrite(const void* data, size_t size, size_t count, FILE* file);

size_t __wrap_fwrite(const void* data, size_t size, size_t count, FILE* file) {
  if (failIO == 1) {
    failIO = 0;
    (void)__real_fwrite(data, 1, size * count > 16 ? 16 : 0, file);
    errno = ENOSPC;
    return 0;
  }

  return __real_fwrite(data, size, count, file);
}

int __real_fflush(FILE* file);

int __wrap_fflush(FILE* file) {
  if (failIO == 2) {
    failIO = 0;
    errno = ENOSPC;
    return EOF;
  }

  return __real_fflush(file);
}

int __real_fclose(FILE* file);

int __wrap_fclose(FILE* file) {
  int result = __real_fclose(file);
  if (failIO == 3) {
    failIO = 0;
    errno = EIO;
    return EOF;
  }

  return result;
}

static uint64_t fingerprint(void) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (int x = -128; x < 128; x++)
    for (int y = 0; y < 64; y++)
      for (int z = -128; z < 128; z++)
        hash = (hash ^ getBlock(&(Vec3i){x, y, z})->id) * UINT64_C(1099511628211);
  return hash;
}

static unsigned char* readFile(const char* path, size_t* size) {
  FILE* f = fopen(path, "rb");
  CHECK(f);
  CHECK(fseek(f, 0, SEEK_END) == 0);
  long length = ftell(f);
  CHECK(length >= 0 && length < 5000000);
  *size = (size_t)length;
  CHECK(fseek(f, 0, SEEK_SET) == 0);
  unsigned char* data = malloc(*size + 1);
  CHECK(data);
  CHECK(fread(data, 1, *size, f) == *size && fclose(f) == 0);
  return data;
}

static void writeFile(const char* path, const unsigned char* bytes, size_t size) {
  FILE* f = fopen(path, "wb");
  CHECK(f);
  CHECK(fwrite(bytes, 1, size, f) == size && fclose(f) == 0);
}

static void put32(unsigned char* p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (unsigned char)(v >> (i * 8));
}

static void fixChecksum(unsigned char* bytes, size_t size) {
  uint32_t hash = UINT32_C(2166136261);
  for (size_t i = 0; i < size; i++)
    if (i < 68 || i >= 72)
      hash = (hash ^ bytes[i]) * UINT32_C(16777619);
  put32(bytes + 68, hash);
}

static void sameFile(const char* path, const unsigned char* expected, size_t size) {
  size_t got;
  unsigned char* data = readFile(path, &got);
  CHECK(got == size && memcmp(data, expected, size) == 0);
  free(data);
}

static void rejected(const char* path, const unsigned char* bytes, size_t size, uint64_t hash) {
  writeFile(path, bytes, size);
  SavedPlayer output = {.feet = {7, 8, 9}, .yaw = 12, .pitch = 13, .selectedSlot = 1}, previous = output;
  char error[256];
  CHECK(loadWorld(path, &output, error, sizeof(error)) != SAVE_OK);
  CHECK(error[0] && memcmp(&output, &previous, sizeof(output)) == 0);
  CHECK(worldSeed() == 42 && fingerprint() == hash);
  sameFile(path, bytes, size);
}

int main(void) {
  char directory[1024], path[1100], blocked[1100], nested[1200], error[256];
#ifdef _WIN32
  const char* root = getenv("TEMP");
#else
  const char* root = getenv("TMPDIR");
#endif
  if (!root)
    root = ".";
  bool created = false;
  for (unsigned i = 0; i < 100; i++) {
    int n = snprintf(directory, sizeof(directory), "%s/kernelcraft-save-%lu-%u", root, (unsigned long)processID(), i);
    CHECK(n > 0 && (size_t)n < sizeof(directory));
    if (makeDirectory(directory) == 0) {
      created = true;
      break;
    }

    CHECK(errno == EEXIST);
  }

  CHECK(created);
  CHECK(snprintf(path, sizeof(path), "%s/world.kcw", directory) > 0);
  CHECK(snprintf(blocked, sizeof(blocked), "%s/blocked", directory) > 0);
  CHECK(snprintf(nested, sizeof(nested), "%s/previous.kcw", blocked) > 0);
  CHECK(initChunksSeeded(42));
  SavedPlayer player = {.feet = {4.6f, 1, 4.5f}, .yaw = 357.5f, .pitch = -35, .selectedSlot = 2};
  CHECK(loadWorld(path, &player, error, sizeof(error)) == SAVE_NOT_FOUND);
  CHECK(setBlock(&(Vec3i){4, 0, 4}, BLOCK_STONE));
  for (int y = 1; y <= 3; y++)
    CHECK(setBlock(&(Vec3i){4, y, 4}, BLOCK_AIR));
  CHECK(setBlock(&(Vec3i){-1, 40, -1}, BLOCK_DIRT));
  CHECK(setBlock(&(Vec3i){0, 40, -1}, BLOCK_GRASS));
  CHECK(setBlock(&(Vec3i){-128, 63, 127}, BLOCK_STONE));
  CHECK(playerCanOccupy(player.feet));
  uint64_t hash = fingerprint();
  CHECK(saveWorld(path, &player, error, sizeof(error)) == SAVE_OK);
  size_t size;
  unsigned char* original = readFile(path, &size);
  CHECK(size == 72 + 256 * 64 * 256 && memcmp(original, "KCRFTSV\0", 8) == 0);
  CHECK(original[8] == 4 && original[60] == 3);
  const int allocationFailures[] = {1, 17, 256};
  for (size_t i = 0; i < sizeof(allocationFailures) / sizeof(allocationFailures[0]); i++) {
    SavedPlayer unchanged = player;
    failAllocation = allocationFailures[i];
    CHECK(loadWorld(path, &unchanged, error, sizeof(error)) == SAVE_NO_MEMORY);
    CHECK(failAllocation == 0 && error[0] && memcmp(&unchanged, &player, sizeof(player)) == 0);
    CHECK(worldSeed() == 42 && fingerprint() == hash);
  }

  CHECK(initChunksSeeded(7));
  SavedPlayer loaded = {0};
  CHECK(loadWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
  CHECK(worldSeed() == 42 && fingerprint() == hash && playerCanOccupy(loaded.feet));
  CHECK(memcmp(&player, &loaded, sizeof(player)) == 0);
  for (int x = 0; x < 16; x++)
    for (int z = 0; z < 16; z++)
      CHECK(getChunk(&(Vec2i){x, z})->dirty);
  CHECK(saveWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
  sameFile(path, original, size);

  unsigned char* bad = malloc(size + 1);
  CHECK(bad);
  // The previous format's material IDs map to the same first three slots.
  for (int material = 1; material <= 3; material++) {
    memcpy(bad, original, size);
    put32(bad + 8, 1);
    put32(bad + 60, (uint32_t)material);
    fixChecksum(bad, size);
    writeFile(path, bad, size);
    CHECK(loadWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
    CHECK(loaded.selectedSlot == material - 1 && loaded.yaw == player.yaw && loaded.pitch == player.pitch);
    CHECK(!memcmp(&loaded.feet, &player.feet, sizeof(player.feet)) && fingerprint() == hash);
  }

  put32(bad + 60, 4);
  fixChecksum(bad, size);
  rejected(path, bad, size, hash);
  // Version 2 supports all nine selected slots, but only the original blocks.
  for (int slot = 1; slot <= 9; slot++) {
    memcpy(bad, original, size);
    put32(bad + 8, 2);
    put32(bad + 60, (uint32_t)slot);
    fixChecksum(bad, size);
    writeFile(path, bad, size);
    CHECK(loadWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
    CHECK(loaded.selectedSlot == slot - 1 && fingerprint() == hash);
  }

  for (int slot = 0; slot < 9; slot++) {
    SavedPlayer selection = player;
    selection.selectedSlot = slot;
    CHECK(saveWorld(path, &selection, error, sizeof(error)) == SAVE_OK);
    CHECK(loadWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
    CHECK(!memcmp(&loaded, &selection, sizeof(loaded)) && fingerprint() == hash);
  }

  // Cobblestone retains its own persisted ID, including at a negative seam.
  CHECK(setBlock(&(Vec3i){-1, 40, -1}, BLOCK_COBBLESTONE));
  SavedPlayer cobblePlayer = player;
  cobblePlayer.selectedSlot = 3;
  uint64_t cobbleHash = fingerprint();
  CHECK(saveWorld(path, &cobblePlayer, error, sizeof(error)) == SAVE_OK);
  size_t cobbleSize;
  unsigned char* cobbleSave = readFile(path, &cobbleSize);
  CHECK(cobbleSize == size && cobbleSave[8] == 4 && cobbleSave[60] == 4);
  put32(cobbleSave + 8, 3);
  fixChecksum(cobbleSave, cobbleSize);
  writeFile(path, cobbleSave, cobbleSize);
  CHECK(setBlock(&(Vec3i){-1, 40, -1}, BLOCK_AIR));
  CHECK(loadWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
  CHECK(getBlock(&(Vec3i){-1, 40, -1})->id == BLOCK_COBBLESTONE && loaded.selectedSlot == 3 && fingerprint() == cobbleHash);
  for (int version = 1; version <= 2; version++) {
    put32(cobbleSave + 8, (uint32_t)version);
    put32(cobbleSave + 60, 3);
    fixChecksum(cobbleSave, cobbleSize);
    rejected(path, cobbleSave, cobbleSize, cobbleHash);
  }

  free(cobbleSave);
  for (int id = 5; id <= 6; id++) {
    CHECK(setBlock(&(Vec3i){-1, 40, -1}, id));
    SavedPlayer builder = player;
    builder.selectedSlot = id - 1;
    uint64_t buildingHash = fingerprint();
    CHECK(saveWorld(path, &builder, error, sizeof(error)) == SAVE_OK);
    size_t buildingSize;
    unsigned char* buildingSave = readFile(path, &buildingSize);
    CHECK(buildingSize == size && buildingSave[8] == 4 && buildingSave[60] == id);
    CHECK(setBlock(&(Vec3i){-1, 40, -1}, BLOCK_AIR));
    CHECK(loadWorld(path, &loaded, error, sizeof(error)) == SAVE_OK);
    CHECK(getBlock(&(Vec3i){-1, 40, -1})->id == id && loaded.selectedSlot == id - 1 && fingerprint() == buildingHash);
    for (int version = 1; version <= 3; version++) {
      put32(buildingSave + 8, (uint32_t)version);
      put32(buildingSave + 60, 3);
      fixChecksum(buildingSave, buildingSize);
      rejected(path, buildingSave, buildingSize, buildingHash);
    }

    free(buildingSave);
  }

  CHECK(setBlock(&(Vec3i){-1, 40, -1}, BLOCK_DIRT) && fingerprint() == hash);

  const struct {
    size_t offset;
    uint32_t value;
  } cases[] = {{0, 0},           {8, 5},  {12, 2},          {20, 512},        {24, 0}, {28, 32}, {32, 255},        {36, UINT32_MAX}, {40, 0x7f7fffff},
               {44, 0x7fc00000}, {44, 0}, {52, 0x43b40000}, {56, 0x42b40000}, {60, 0}, {60, 10}, {60, UINT32_MAX}, {64, 1},          {72, 255}};

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    memcpy(bad, original, size);
    put32(bad + cases[i].offset, cases[i].value);
    fixChecksum(bad, size);
    rejected(path, bad, size, hash);
  }

  memcpy(bad, original, size);
  bad[68] ^= 1;
  rejected(path, bad, size, hash);
  const size_t lengths[] = {0, 7, 71, 72, 4194375};
  for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
    rejected(path, original, lengths[i], hash);
  memcpy(bad, original, size);
  bad[size] = 1;
  rejected(path, bad, size + 1, hash);
  writeFile(path, original, size);
  SavedPlayer invalid = player;
  invalid.feet.y = NAN;
  CHECK(saveWorld(path, &invalid, error, sizeof(error)) == SAVE_INVALID);
  sameFile(path, original, size);
  for (int slot = -1; slot <= 9; slot += 10) {
    invalid = player;
    invalid.selectedSlot = slot;
    CHECK(saveWorld(path, &invalid, error, sizeof(error)) == SAVE_INVALID);
    sameFile(path, original, size);
  }

  CHECK(setBlock(&(Vec3i){-1, 40, -1}, BLOCK_STONE));
  for (int failure = 1; failure <= 5; failure++) {
    failIO = failure;
    CHECK(saveWorld(path, &player, error, sizeof(error)) == SAVE_IO_ERROR);
    CHECK(failIO == 0 && error[0]);
    sameFile(path, original, size);
  }

  CHECK(makeDirectory(blocked) == 0);
  writeFile(nested, original, size);
  CHECK(saveWorld(blocked, &player, error, sizeof(error)) == SAVE_IO_ERROR);
  sameFile(nested, original, size);
  CHECK(remove(nested) == 0 && removeDirectory(blocked) == 0);
  CHECK(remove(path) == 0 && removeDirectory(directory) == 0); // Also catches leaked sibling temp files.
  free(bad);
  free(original);
  cleanupChunks();
  puts("Save round-trip, malformed data, staged load, and failed replacement tests passed");
  return 0;
}
