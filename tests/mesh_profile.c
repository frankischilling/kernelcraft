// Standalone CPU measurement; link with the same world sources as test-world.
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "world/world.h"
#include "world/mesh.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

static double profileNow(void) {
#ifdef _WIN32
  LARGE_INTEGER counter, frequency;
  QueryPerformanceCounter(&counter);
  QueryPerformanceFrequency(&frequency);
  return (double)counter.QuadPart / frequency.QuadPart;
#else
  struct timespec value;
  clock_gettime(CLOCK_MONOTONIC, &value);
  return value.tv_sec + value.tv_nsec * 1e-9;
#endif
}

int main(void) {
  if (!initChunksSeeded(0))
    return 1;
  const char* scenes[] = {"generated", "empty", "sparse", "solid", "checkerboard"};
  Chunk* chunk = getChunk(&(Vec2i){8, 8});
  puts("scenario,mean_ms,vertices,indices,surface_blocks");
  for (int scene = 0; scene < 5; scene++) {
    if (scene == 1)
      for (int x = 0; x < CHUNKS_PER_AXIS; x++)
        for (int z = 0; z < CHUNKS_PER_AXIS; z++)
          memset(getChunk(&(Vec2i){x, z})->blocks, 0, sizeof(chunk->blocks));
    if (scene > 1)
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_HEIGHT; y++)
          for (int z = 0; z < CHUNK_SIZE; z++) {
            bool solid = scene == 2 ? x == 8 && y == 32 && z == 8 : scene == 3 || ((x + y + z) & 1);
            chunk->blocks[x][y][z].id = solid ? BLOCK_STONE : BLOCK_AIR;
          }
    double start = 0;
    size_t vertices = 0, indices = 0;
    int surface = 0;
    for (int i = -8; i < 128; i++) {
      if (!i)
        start = profileNow();
      ChunkMesh mesh;
      if (!buildChunkMesh(chunk, &mesh)) {
        cleanupChunks();
        return 2;
      }
      vertices = mesh.vertexCount;
      indices = mesh.indexCount;
      surface = mesh.surfaceBlocks;
      freeChunkMesh(&mesh);
    }
    double ms = (profileNow() - start) * 1000 / 128;
    printf("%s,%.6f,%zu,%zu,%d\n", scenes[scene], ms, vertices, indices, surface);
  }
  cleanupChunks();
  return 0;
}
