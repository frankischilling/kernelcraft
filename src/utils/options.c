#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static bool fail(char* error, size_t capacity, const char* message) {
  if (error && capacity)
    snprintf(error, capacity, "%s", message);
  return false;
}

bool parseOptions(int argc, char* const argv[], AppOptions* options, char* error, size_t capacity) {
  if (error && capacity)
    error[0] = 0;
  if (!options || argc < 1 || !argv)
    return fail(error, capacity, "Invalid command line");
  *options = (AppOptions){0};
  const char* path = "kernelcraft.kcw";
  bool worldGiven = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      options->help = true;
    } else if (!strcmp(argv[i], "--no-save")) {
      if (options->noSave)
        return fail(error, capacity, "Duplicate --no-save");
      options->noSave = true;
    } else if (!strcmp(argv[i], "--world")) {
      if (worldGiven || ++i == argc || !argv[i][0])
        return fail(error, capacity, "--world needs one nonempty path");
      path = argv[i];
      worldGiven = true;
    } else if (!strcmp(argv[i], "--seed")) {
      if (options->seedGiven || ++i == argc || !argv[i][0])
        return fail(error, capacity, "--seed needs one decimal integer from 0 to 4294967295");
      uint32_t seed = 0;
      for (const char* p = argv[i]; *p; p++) {
        if (*p < '0' || *p > '9' || seed > (UINT32_MAX - (unsigned)(*p - '0')) / 10)
          return fail(error, capacity, "--seed must be a decimal integer from 0 to 4294967295");
        seed = seed * 10 + (unsigned)(*p - '0');
      }
      options->seed = seed;
      options->seedGiven = true;
    } else {
      return fail(error, capacity, "Unknown argument; use --help for usage");
    }
  }
  if (worldGiven && options->noSave)
    return fail(error, capacity, "--world and --no-save cannot be combined");
  if (options->help || options->noSave)
    return true;
#ifdef _WIN32
  if (strlen(path) >= sizeof(options->worldPath) || !_fullpath(options->worldPath, path, sizeof(options->worldPath)))
    return fail(error, capacity, "Cannot resolve world path; check its length and launch directory");
#else
  if (path[0] == '/') {
    if (strlen(path) >= sizeof(options->worldPath))
      return fail(error, capacity, "World path is too long");
    strcpy(options->worldPath, path);
  } else {
    char directory[WORLD_PATH_CAPACITY];
    if (!getcwd(directory, sizeof(directory)))
      return fail(error, capacity, "Cannot resolve the launch directory");
    int length = snprintf(options->worldPath, sizeof(options->worldPath), "%s/%s", directory, path);
    if (length < 0 || (size_t)length >= sizeof(options->worldPath))
      return fail(error, capacity, "World path is too long");
  }
#endif
  return true;
}
