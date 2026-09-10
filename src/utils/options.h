#ifndef OPTIONS_H
#define OPTIONS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WORLD_PATH_CAPACITY 4096

typedef struct {
  uint32_t seed;
  bool seedGiven, noSave, help;
  char worldPath[WORLD_PATH_CAPACITY];
} AppOptions;

// Resolve relative save paths before the application changes its asset directory.
bool parseOptions(int argc, char* const argv[], AppOptions* options, char* error, size_t capacity);
#endif
