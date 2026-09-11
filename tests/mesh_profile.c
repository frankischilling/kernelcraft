// Standalone CPU measurement; link with the same world sources as test-world.
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "world/world.h"
#include "world/mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

// Optional requested-allocation accounting; run separately from timing trials.
#ifdef KERNELCRAFT_PROFILE_ALLOCATIONS
static void* allocations[16];
static size_t allocationSizes[16], liveBytes, peakBytes, allocationCalls;
static bool trackAllocations;
void* __real_malloc(size_t size);
void __real_free(void* pointer);

void* __wrap_malloc(size_t size) {
  void* pointer = __real_malloc(size);
  if (trackAllocations && pointer) {
    size_t i = 0;
    while (i < 16 && allocations[i])
      i++;
    if (i == 16)
      abort();
    allocations[i] = pointer;
    allocationSizes[i] = size;
    allocationCalls++;
    liveBytes += size;
    if (liveBytes > peakBytes)
      peakBytes = liveBytes;
  }
  return pointer;
}

void __wrap_free(void* pointer) {
  if (pointer)
    for (size_t i = 0; i < 16; i++)
      if (allocations[i] == pointer) {
        liveBytes -= allocationSizes[i];
        allocations[i] = NULL;
        break;
      }
  __real_free(pointer);
}
#endif

static int compareTimes(const void* a, const void* b) {
  double left = *(const double*)a, right = *(const double*)b;
  return (left > right) - (left < right);
}

static uint64_t hashBytes(uint64_t hash, const void* data, size_t size) {
  const unsigned char* bytes = data;
  for (size_t i = 0; i < size; i++)
    hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
  return hash;
}

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
  puts("scenario,mean_ms,median_ms,p95_ms,p99_ms,vertices,indices,surface_blocks,mesh_hash");
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
    double times[128], total = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t vertices = 0, indices = 0;
    int surface = 0;
#ifdef KERNELCRAFT_PROFILE_ALLOCATIONS
    peakBytes = allocationCalls = 0;
    trackAllocations = true;
#endif
    for (int i = -8; i < 128; i++) {
      double start = profileNow();
      ChunkMesh mesh;
      if (!buildChunkMesh(chunk, &mesh)) {
        cleanupChunks();
        return 2;
      }
      vertices = mesh.vertexCount;
      indices = mesh.indexCount;
      surface = mesh.surfaceBlocks;
      // Fingerprint outside the measured interval on the final warm-up build.
      if (i == -1) {
        hash = hashBytes(hash, mesh.vertices, mesh.vertexCount * sizeof(*mesh.vertices));
        hash = hashBytes(hash, mesh.indices, mesh.indexCount * sizeof(*mesh.indices));
        hash = hashBytes(hash, mesh.batches, sizeof(mesh.batches));
        hash = hashBytes(hash, &mesh.min, sizeof(mesh.min));
        hash = hashBytes(hash, &mesh.max, sizeof(mesh.max));
      }
      freeChunkMesh(&mesh);
      if (i >= 0) {
        times[i] = (profileNow() - start) * 1000;
        total += times[i];
      }
    }
    qsort(times, 128, sizeof(*times), compareTimes);
    printf("%s,%.6f,%.6f,%.6f,%.6f,%zu,%zu,%d,%llu\n", scenes[scene], total / 128, (times[63] + times[64]) * 0.5, times[121], times[126], vertices, indices, surface,
           (unsigned long long)hash);
#ifdef KERNELCRAFT_PROFILE_ALLOCATIONS
    trackAllocations = false;
    fprintf(stderr, "ALLOCATIONS scene=%s calls_per_build=%zu peak_requested_bytes=%zu live_bytes=%zu\n", scenes[scene], allocationCalls / 136, peakBytes, liveBytes);
    if (liveBytes)
      return 3;
#endif
  }
  cleanupChunks();
  return 0;
}
