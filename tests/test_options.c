#include "utils/options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "Options test: %s (line %d)\n", #c, __LINE__);                                                                                                               \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)
static bool parse(int count, char* const args[], AppOptions* options) {
  char error[256];
  bool okay = parseOptions(count, args, options, error, sizeof(error));
  CHECK(okay ? !error[0] : error[0]);
  return okay;
}
int main(void) {
  AppOptions options;
  CHECK(parse(1, (char*[]){"game"}, &options));
  CHECK(options.seed == 0 && !options.seedGiven && !options.noSave && !options.help);
  CHECK(strstr(options.worldPath, "kernelcraft.kcw") && options.worldPath[0]);
  CHECK(parse(5, (char*[]){"game", "--world", "world with spaces.kcw", "--seed", "4294967295"}, &options));
  CHECK(options.seed == UINT32_MAX && options.seedGiven && strstr(options.worldPath, "world with spaces.kcw"));
  CHECK(parse(4, (char*[]){"game", "--seed", "00042", "--no-save"}, &options));
  CHECK(options.noSave && options.seed == 42);
  CHECK(parse(2, (char*[]){"game", "--help"}, &options) && options.help);
  const char* badSeeds[] = {"", "-1", "+1", " 1", "1 ", "0x10", "4294967296", "9999999999999999999999", "12cats"};
  for (size_t i = 0; i < sizeof(badSeeds) / sizeof(badSeeds[0]); i++)
    CHECK(!parse(3, (char*[]){"game", "--seed", (char*)badSeeds[i]}, &options));
  CHECK(!parse(2, (char*[]){"game", "--seed"}, &options));
  CHECK(!parse(2, (char*[]){"game", "--world"}, &options));
  CHECK(!parse(3, (char*[]){"game", "--world", ""}, &options));
  CHECK(!parse(2, (char*[]){"game", "--unknown"}, &options));
  CHECK(!parse(5, (char*[]){"game", "--seed", "1", "--seed", "2"}, &options));
  CHECK(!parse(4, (char*[]){"game", "--world", "x", "--no-save"}, &options));
  CHECK(!parse(3, (char*[]){"game", "--no-save", "--no-save"}, &options));
  char tooLong[5000];
  memset(tooLong, 'x', sizeof(tooLong) - 1);
  tooLong[sizeof(tooLong) - 1] = 0;
  CHECK(!parse(3, (char*[]){"game", "--world", tooLong}, &options));
  puts("World path, seed, and command-line validation tests passed");
  return 0;
}
