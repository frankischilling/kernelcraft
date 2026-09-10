// Optional measurement mode for render_benchmark.c. Compile this identical
// header against both revisions; no production profiling hooks.
#include "world/player.h"
#include <math.h>

enum { PROFILE_WARMUP = 120, PROFILE_FRAMES = 600 };
static size_t profileUploadBytes;
static unsigned profileQueries, profileQueryObjects;
static PFNGLBUFFERDATAPROC profilePreviousBufferData;
static PFNGLBEGINQUERYPROC profilePreviousBeginQuery;
static PFNGLGENQUERIESPROC profilePreviousGenQueries;

typedef struct {
  double frameMs, cpuMs, rebuildMs;
  size_t triangles, uploadBytes;
  int draws, surfaceBlocks, rebuilt;
  unsigned queries;
  unsigned long uploadCalls;
} ProfileFrame;

static void GLAPIENTRY profileBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  if (size > 0)
    profileUploadBytes += (size_t)size;
  profilePreviousBufferData(target, size, data, usage);
}

static void GLAPIENTRY profileBeginQuery(GLenum target, GLuint query) {
  if (target == GL_ANY_SAMPLES_PASSED)
    profileQueries++;
  profilePreviousBeginQuery(target, query);
}

static void GLAPIENTRY profileGenQueries(GLsizei count, GLuint* queries) {
  profileQueryObjects += (unsigned)count;
  profilePreviousGenQueries(count, queries);
}

static int profileCompare(const void* left, const void* right) {
  double a = *(const double*)left, b = *(const double*)right;
  return (a > b) - (a < b);
}

static int profileRendering(GLuint shader) {
  bool pipelined = getenv("KERNELCRAFT_PROFILE_PIPELINED") != NULL;
  GLFWwindow* window = glfwGetCurrentContext();
  glfwSwapInterval(0);
  glfwSetWindowSize(window, 1280, 720);
  glfwPollEvents();
  glfwSwapBuffers(window);
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  if (width != 1280 || height != 720)
    return 30;
  glViewport(0, 0, width, height);
  printf("PROFILE_ENV renderer=%s version=%s resolution=%dx%d warmup=%d frames=%d seed=0 mode=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION), width, height,
         PROFILE_WARMUP, PROFILE_FRAMES, pipelined ? "pipelined" : "serialized");
  const char* rawPath = getenv("KERNELCRAFT_PROFILE_CSV");
  FILE* raw = rawPath ? fopen(rawPath, "wb") : NULL;
  if (rawPath && !raw)
    return 31;
  if (raw)
    fputs("scenario,frame,frame_ms,cpu_submit_ms,gpu_ms,terrain_draws,triangles,surface_blocks,queries,rebuild_ms,chunks_rebuilt,upload_calls,upload_bytes\n", raw);
  profilePreviousBufferData = __glewBufferData;
  profilePreviousBeginQuery = __glewBeginQuery;
  profilePreviousGenQueries = __glewGenQueries;
  __glewBufferData = profileBufferData;
  __glewBeginQuery = profileBeginQuery;
  __glewGenQueries = profileGenQueries;
  cleanupWorld();
  double start = glfwGetTime();
  bool success = initChunksSeeded(0);
  double generationMs = (glfwGetTime() - start) * 1000;
  start = glfwGetTime();
  success = success && initWorld(shader);
  glFinish();
  printf("PROFILE_INIT generation_ms=%.6f mesh_upload_ms=%.6f geometry_upload_bytes=%zu query_objects=%u cpu_block_bytes=%zu\n", generationMs, (glfwGetTime() - start) * 1000,
         profileUploadBytes, profileQueryObjects, sizeof(Block) * WORLD_BLOCK_COUNT);
  GLuint timers[PROFILE_FRAMES + 1];
  int timerCount = pipelined ? PROFILE_FRAMES + 1 : 1;
  profilePreviousGenQueries(timerCount, timers);
  Player player = {0};
  success = success && playerFindSpawn(&player, (Vec3){0, 0, 3});
  Vec3 eye = playerEyePosition(&player);
  printf("PROFILE_SPAWN x=%.3f y=%.3f z=%.3f\n", eye.x, eye.y, eye.z);
  const char* scenarios[] = {"underground", "surface_still", "overview", "sky", "translate", "rotate", "seam_edits", "sky_moving", "wall_moving"};
  // One changing block on an X chunk seam, visible from the spawned eye.
  Vec3i edit = {-1, (int)floorf(player.position.y), 6};
  const Block* original = getBlock(&edit);
  int originalID = original ? original->id : BLOCK_AIR;
  int changedID = originalID == BLOCK_AIR ? BLOCK_STONE : BLOCK_AIR;
  for (size_t scenario = 0; success && scenario < sizeof(scenarios) / sizeof(scenarios[0]); scenario++) {
    double frameTimes[PROFILE_FRAMES], cpuTimes[PROFILE_FRAMES], gpuTimes[PROFILE_FRAMES];
    ProfileFrame samples[PROFILE_FRAMES];
    double drainMs = 0, batchStart = 0, frameBoundary = 0;
    double totalFrame = 0, totalCPU = 0, totalGPU = 0, rebuildMs = 0;
    size_t totalTriangles = 0, totalBytes = 0;
    unsigned long totalDraws = 0, totalQueries = 0, totalRebuilt = 0, totalUploads = 0, totalBlocks = 0;
    // Add a tall player-built wall in front of the same generated terrain.
    // Camera motion behind it exercises actual occlusion, not frustum rejection.
    if (scenario == 8)
      for (int x = 0; x < 16; x++)
        for (int y = 1; y < 32; y++)
          success = setBlock(&(Vec3i){x, y, 8}, BLOCK_STONE) && success;
    // Reset visibility identically on both revisions outside the timed frames.
    success = initWorld(shader);
    glFinish();
    for (int frame = -PROFILE_WARMUP; success && frame < PROFILE_FRAMES; frame++) {
      if (pipelined && frame == 0) {
        glFinish();
        batchStart = frameBoundary = glfwGetTime();
      }
      Camera camera;
      initCamera(&camera);
      camera.position = eye;
      camera.pitch = -10;
      if (scenario == 0)
        camera.position = (Vec3){0, 10, 3};
      if (scenario == 2) {
        camera.position = (Vec3){0, 32, 3};
        camera.pitch = -45;
      }
      if (scenario == 3) {
        camera.position.y = 40;
        camera.pitch = 89;
      }
      // Repeat exactly the same path during warm-up and each measured trial.
      int step = frame < 0 ? frame + PROFILE_WARMUP : frame;
      if (scenario == 4)
        camera.position.x += step * 0.015f;
      if (scenario == 5)
        camera.yaw += step * 0.3f;
      if (scenario == 7) {
        // Repeat the sky-motion regression at the spawn height, where chunk
        // boxes overlap the view even though their terrain surfaces do not.
        camera.pitch = 89;
        camera.position.x += (step % 12) * 0.0025f;
        camera.yaw += (step % 12) * 0.05f;
      }
      if (scenario == 8)
        camera.position = (Vec3){8.5f + sinf(step * 0.01f), 20, 3.5f};
      updateCameraVectors(&camera);
      if (scenario == 6)
        success = setBlock(&edit, step % 2 ? originalID : changedID);
      Mat4 view, projection;
      Vec3 target;
      vec3_add(&target, &camera.position, &camera.front);
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      mat4_perspective(projection, 70, (float)width / height, 0.1f, 1000);
      profileQueries = 0;
      profileUploadBytes = 0;
      draws = uploads = lookups = 0;
      // Begin measurement with no warm-up work left in the GPU queue.
      if (!pipelined && frame == 0)
        glFinish();
      GLuint timer = timers[pipelined && frame >= 0 ? frame + 1 : 0];
      start = glfwGetTime();
      glBeginQuery(GL_TIME_ELAPSED, timer);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      RenderResult result = renderWorld(&camera, view, projection, false);
      DebugData data = {.camera = &camera, .fps = 60, .visibleBlocks = result.surfaceBlocks, .captured = true, .selectedSlot = 0, .stats = &result};
      data.selection = rayCast(camera.position, camera.front, EDIT_REACH);
      drawSelection(&data.selection, view, projection);
      HUDDraw(shader, &data); // Normal HUD; F3 text is hidden on both revisions.
      glEndQuery(GL_TIME_ELAPSED);
      double cpuMs = (glfwGetTime() - start) * 1000;
      if (pipelined) {
        glfwSwapBuffers(window);
        // Include completion of the entire measured batch, rather than
        // reporting only how fast the CPU can enqueue unfinished frames.
        if (frame == PROFILE_FRAMES - 1) {
          double drainStart = glfwGetTime();
          glFinish();
          drainMs = (glfwGetTime() - drainStart) * 1000;
        }
      } else {
        glFinish();
      }
      double frameMs = (glfwGetTime() - start) * 1000;
      double gpuMs = 0;
      if (!pipelined) {
        GLuint64 nanoseconds;
        glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &nanoseconds);
        gpuMs = (double)nanoseconds / 1000000;
      }
      success = success && result.success && glGetError() == GL_NO_ERROR && isfinite(frameMs) && frameMs > 0 && result.terrainDrawCalls == result.chunksRendered &&
                draws == (unsigned long)result.terrainDrawCalls + 1;
      if (scenario != 6)
        success = success && result.chunksRebuilt == 0 && uploads == 0 && lookups == 0;
      else
        success = success && result.chunksRebuilt == 2;
      if (!success)
        fprintf(stderr, "Profile invariant failed: scenario=%s frame=%d draws=%lu terrain=%d rebuilt=%d uploads=%lu\n", scenarios[scenario], frame, draws, result.terrainDrawCalls,
                result.chunksRebuilt, uploads);
      if (frame < 0)
        continue;
      if (pipelined) {
        // Contiguous intervals include setup and inter-frame bookkeeping.
        // Their sum is the wall time for the completed measured batch.
        double boundary = glfwGetTime();
        frameMs = (boundary - frameBoundary) * 1000;
        frameBoundary = boundary;
      }
      frameTimes[frame] = frameMs;
      cpuTimes[frame] = cpuMs;
      gpuTimes[frame] = gpuMs;
      totalFrame += frameMs;
      totalCPU += cpuMs;
      rebuildMs += result.meshUpdateMilliseconds;
      totalDraws += result.terrainDrawCalls;
      totalTriangles += result.submittedTriangles;
      totalBlocks += result.surfaceBlocks;
      totalQueries += profileQueries;
      totalRebuilt += result.chunksRebuilt;
      totalUploads += uploads;
      totalBytes += profileUploadBytes;
      samples[frame] = (ProfileFrame){
          frameMs,        cpuMs,  result.meshUpdateMilliseconds, result.submittedTriangles, profileUploadBytes, result.terrainDrawCalls, result.surfaceBlocks, result.chunksRebuilt,
          profileQueries, uploads};
    }
    if (!success)
      break;
    // Queue completion above makes every timer ready. Defer both readback
    // and CSV writes so they cannot serialize pipelined measured frames.
    for (int frame = 0; frame < PROFILE_FRAMES; frame++) {
      if (pipelined) {
        GLuint64 nanoseconds;
        glGetQueryObjectui64v(timers[frame + 1], GL_QUERY_RESULT, &nanoseconds);
        gpuTimes[frame] = (double)nanoseconds / 1000000;
      }
      totalGPU += gpuTimes[frame];
      const ProfileFrame* sample = &samples[frame];
      if (raw)
        fprintf(raw, "%s,%d,%.6f,%.6f,%.6f,%d,%zu,%d,%u,%.6f,%d,%lu,%zu\n", scenarios[scenario], frame, sample->frameMs, sample->cpuMs, gpuTimes[frame], sample->draws,
                sample->triangles, sample->surfaceBlocks, sample->queries, sample->rebuildMs, sample->rebuilt, sample->uploadCalls, sample->uploadBytes);
    }
    success = glGetError() == GL_NO_ERROR;
    if (!success)
      break;
    if (pipelined)
      printf("PROFILE_PIPELINE scenario=%s batch_ms=%.6f final_drain_ms=%.6f\n", scenarios[scenario], (frameBoundary - batchStart) * 1000, drainMs);
    qsort(frameTimes, PROFILE_FRAMES, sizeof(double), profileCompare);
    qsort(cpuTimes, PROFILE_FRAMES, sizeof(double), profileCompare);
    qsort(gpuTimes, PROFILE_FRAMES, sizeof(double), profileCompare);
    double slowest = 0;
    for (int frame = PROFILE_FRAMES - PROFILE_FRAMES / 100; frame < PROFILE_FRAMES; frame++)
      slowest += frameTimes[frame];
    printf("PROFILE_RESULT %s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", scenarios[scenario], 1000 * PROFILE_FRAMES / totalFrame,
           totalFrame / PROFILE_FRAMES, frameTimes[PROFILE_FRAMES / 2 - 1] * 0.5 + frameTimes[PROFILE_FRAMES / 2] * 0.5, frameTimes[(PROFILE_FRAMES * 95 + 99) / 100 - 1],
           frameTimes[(PROFILE_FRAMES * 99 + 99) / 100 - 1], 1000 * (PROFILE_FRAMES / 100) / slowest, totalCPU / PROFILE_FRAMES, totalGPU / PROFILE_FRAMES,
           (double)totalDraws / PROFILE_FRAMES, (double)totalTriangles / PROFILE_FRAMES, (double)totalBlocks / PROFILE_FRAMES, (double)totalQueries / PROFILE_FRAMES,
           rebuildMs / PROFILE_FRAMES, (double)totalRebuilt / PROFILE_FRAMES, (double)totalUploads / PROFILE_FRAMES, (double)totalBytes / PROFILE_FRAMES,
           gpuTimes[(PROFILE_FRAMES * 99 + 99) / 100 - 1]);
  }
  setBlock(&edit, originalID);
  glDeleteQueries(timerCount, timers);
  __glewBufferData = profilePreviousBufferData;
  __glewBeginQuery = profilePreviousBeginQuery;
  __glewGenQueries = profilePreviousGenQueries;
  if (raw && fclose(raw) != 0)
    success = false;
  if (!success)
    fprintf(stderr, "Rendering profile failed its GL, draw-count, or rebuild checks\n");
  return success ? 0 : 32;
}
