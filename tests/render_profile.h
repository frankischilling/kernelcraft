// Optional measurement mode for render_benchmark.c. Compile this identical
// header against both revisions; no production profiling hooks.
#include "world/player.h"
#include "utils/text.h"
#include <math.h>

static bool profileSkipText;

void __real_renderText(const TextState* state, const char* text, float x, float y);

void __wrap_renderText(const TextState* state, const char* text, float x, float y) {
  if (!profileSkipText)
    __real_renderText(state, text, x, y);
}

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
  const char* widthText = getenv("KERNELCRAFT_PROFILE_WIDTH");
  const char* heightText = getenv("KERNELCRAFT_PROFILE_HEIGHT");
  char *widthEnd = NULL, *heightEnd = NULL;
  long requestedWidth = widthText ? strtol(widthText, &widthEnd, 10) : 1280;
  long requestedHeight = heightText ? strtol(heightText, &heightEnd, 10) : 720;
  if ((widthText && (widthEnd == widthText || *widthEnd)) || (heightText && (heightEnd == heightText || *heightEnd)) || requestedWidth < 1 || requestedWidth > 8192 ||
      requestedHeight < 1 || requestedHeight > 8192)
    return 30;
  bool atmosphere = getenv("KERNELCRAFT_PROFILE_ATMOSPHERE") != NULL;
  const char* phaseText = getenv("KERNELCRAFT_PROFILE_PHASE");
  char* phaseEnd = NULL;
  double phase = phaseText ? strtod(phaseText, &phaseEnd) : 0.125;
  if ((phaseText && (phaseEnd == phaseText || *phaseEnd)) || !isfinite(phase) || phase < 0 || phase >= 1)
    return 33;
  SkyRenderer sky = {0};
  CloudRenderer clouds = {0};
  if (atmosphere && (!initSky(&sky) || !initClouds(&clouds))) {
    cleanupClouds(&clouds);
    cleanupSky(&sky);
    return 34;
  }
  bool pipelined = getenv("KERNELCRAFT_PROFILE_PIPELINED") != NULL;
  bool debug = getenv("KERNELCRAFT_PROFILE_DEBUG") != NULL;
  const char* onlyScene = getenv("KERNELCRAFT_PROFILE_SCENE");
  profileSkipText = getenv("KERNELCRAFT_PROFILE_SKIP_TEXT") != NULL;
  printf("PROFILE_HUD debug=%d skip_text=%d scene=%s\n", debug, profileSkipText, onlyScene ? onlyScene : "all");
  printf("PROFILE_ATMOSPHERE enabled=%d phase=%.6f\n", atmosphere, phase);
  GLFWwindow* window = glfwGetCurrentContext();
  glfwSwapInterval(0);
  // Decorations can clamp a 1080-high client area on a 1080-high desktop.
  // This hidden profiling window must keep the requested framebuffer size.
  glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
  glfwSetWindowSize(window, (int)requestedWidth, (int)requestedHeight);
  glfwPollEvents();
  glfwSwapBuffers(window);
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  if (width != requestedWidth || height != requestedHeight) {
    fprintf(stderr, "Profile framebuffer mismatch: requested=%ldx%ld actual=%dx%d\n", requestedWidth, requestedHeight, width, height);
    return 30;
  }
  glViewport(0, 0, width, height);
  printf("PROFILE_ENV renderer=%s version=%s resolution=%dx%d warmup=%d frames=%d seed=0 mode=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION), width, height,
         PROFILE_WARMUP, PROFILE_FRAMES, pipelined ? "pipelined" : "serialized");
  const char* rawPath = getenv("KERNELCRAFT_PROFILE_CSV");
  FILE* raw = rawPath ? fopen(rawPath, "wb") : NULL;
  if (rawPath && !raw)
    return 31;
  if (raw)
    fputs("scenario,frame,frame_ms,cpu_submit_ms,gpu_ms,terrain_draws,triangles,surface_blocks,queries,rebuild_ms,chunks_rebuilt,upload_calls,upload_bytes,cloud_gpu_ms,cloud_cpu_"
          "ms\n",
          raw);
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
  GLuint cloudTimers[PROFILE_FRAMES * 2];
  if (atmosphere)
    profilePreviousGenQueries(PROFILE_FRAMES * 2, cloudTimers);
  Player player = {0};
  success = success && playerFindSpawn(&player, (Vec3){0, 0, 3});
  Vec3 eye = playerEyePosition(&player);
  printf("PROFILE_SPAWN x=%.3f y=%.3f z=%.3f\n", eye.x, eye.y, eye.z);
  const char* scenarios[] = {"underground", "surface_still", "overview", "sky", "translate", "rotate", "seam_edits", "sky_moving", "wall_moving", "cloud_layer"};
  // One changing block on an X chunk seam, visible from the spawned eye.
  Vec3i edit = {-1, (int)floorf(player.position.y), 6};
  const Block* original = getBlock(&edit);
  int originalID = original ? original->id : BLOCK_AIR;
  int changedID = originalID == BLOCK_AIR ? BLOCK_STONE : BLOCK_AIR;
  bool matchedScene = false;
  for (size_t scenario = 0; success && scenario < sizeof(scenarios) / sizeof(scenarios[0]); scenario++) {
    if (onlyScene && strcmp(onlyScene, scenarios[scenario]))
      continue;
    matchedScene = true;
    double frameTimes[PROFILE_FRAMES], cpuTimes[PROFILE_FRAMES], gpuTimes[PROFILE_FRAMES];
    double cloudGPU[PROFILE_FRAMES] = {0}, cloudCPU[PROFILE_FRAMES] = {0};
    double totalCloudGPU = 0, totalCloudCPU = 0;
    unsigned long totalCloudDraws = 0;
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
      if (scenario == 9) {
        camera.position = (Vec3){step * 0.75f - 128, 122, 3.5f};
        camera.pitch = 0;
      }
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
      DayNightState daylight = {0};
      if (atmosphere) {
        daylight = sampleDayNight(phase + step / 72000.0);
        renderSky(&sky, &camera, (float)width / height, &daylight);
        setWorldDayNight(&daylight);
      }
      RenderResult result = renderWorld(&camera, view, projection, false);
      DebugData data = {
          .camera = &camera, .fps = 60 + (step % 100) * 0.1f, .visibleBlocks = result.surfaceBlocks, .captured = true, .selectedSlot = 0, .stats = &result, .showDebug = debug};
      data.selection = rayCast(camera.position, camera.front, EDIT_REACH);
      drawSelection(&data.selection, view, projection);
      unsigned long cloudDraws = 0;
      if (atmosphere) {
        clouds.offset = step * 0.01;
        double cloudStart = glfwGetTime();
        unsigned long beforeClouds = draws;
        if (frame >= 0)
          glQueryCounter(cloudTimers[frame * 2], GL_TIMESTAMP);
        renderClouds(&clouds, &camera, (float)width / height, projection, &daylight);
        if (frame >= 0) {
          glQueryCounter(cloudTimers[frame * 2 + 1], GL_TIMESTAMP);
          cloudCPU[frame] = (glfwGetTime() - cloudStart) * 1000;
        }
        cloudDraws = draws - beforeClouds;
      }
      HUDDraw(shader, &data);
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
                cloudDraws <= 1 && draws == (unsigned long)result.terrainDrawCalls + 1 + (atmosphere ? 1 : 0) + cloudDraws;
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
      totalCloudDraws += cloudDraws;
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
      if (atmosphere) {
        GLuint64 begin, end;
        glGetQueryObjectui64v(cloudTimers[frame * 2], GL_QUERY_RESULT, &begin);
        glGetQueryObjectui64v(cloudTimers[frame * 2 + 1], GL_QUERY_RESULT, &end);
        cloudGPU[frame] = (double)(end - begin) / 1000000;
      }
      totalCloudGPU += cloudGPU[frame];
      totalCloudCPU += cloudCPU[frame];
      const ProfileFrame* sample = &samples[frame];
      if (raw)
        fprintf(raw, "%s,%d,%.6f,%.6f,%.6f,%d,%zu,%d,%u,%.6f,%d,%lu,%zu,%.6f,%.6f\n", scenarios[scenario], frame, sample->frameMs, sample->cpuMs, gpuTimes[frame], sample->draws,
                sample->triangles, sample->surfaceBlocks, sample->queries, sample->rebuildMs, sample->rebuilt, sample->uploadCalls, sample->uploadBytes, cloudGPU[frame],
                cloudCPU[frame]);
    }

    success = glGetError() == GL_NO_ERROR;
    if (!success)
      break;
    if (pipelined)
      printf("PROFILE_PIPELINE scenario=%s batch_ms=%.6f final_drain_ms=%.6f\n", scenarios[scenario], (frameBoundary - batchStart) * 1000, drainMs);
    qsort(frameTimes, PROFILE_FRAMES, sizeof(double), profileCompare);
    qsort(cpuTimes, PROFILE_FRAMES, sizeof(double), profileCompare);
    qsort(gpuTimes, PROFILE_FRAMES, sizeof(double), profileCompare);
    qsort(cloudGPU, PROFILE_FRAMES, sizeof(double), profileCompare);
    printf("PROFILE_CLOUD %s,%.6f,%.6f,%.6f,%.6f,%.6f\n", scenarios[scenario], totalCloudGPU / PROFILE_FRAMES, cloudGPU[(PROFILE_FRAMES * 95 + 99) / 100 - 1],
           cloudGPU[(PROFILE_FRAMES * 99 + 99) / 100 - 1], totalCloudCPU / PROFILE_FRAMES, (double)totalCloudDraws / PROFILE_FRAMES);
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
  if (atmosphere)
    glDeleteQueries(PROFILE_FRAMES * 2, cloudTimers);
  __glewBufferData = profilePreviousBufferData;
  __glewBeginQuery = profilePreviousBeginQuery;
  __glewGenQueries = profilePreviousGenQueries;
  if (raw && fclose(raw) != 0)
    success = false;
  success = success && matchedScene;
  profileSkipText = false;
  cleanupClouds(&clouds);
  cleanupSky(&sky);
  if (!success)
    fprintf(stderr, "Rendering profile failed its GL, draw-count, or rebuild checks\n");
  return success ? 0 : 32;
}
