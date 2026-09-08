#include <GL/glew.h>
#include "graphics/camera.h"
#include "graphics/hud.h"
#include "graphics/shader.h"
#include "world/world.h"
#ifndef KERNELCRAFT_BASELINE
#include "graphics/world_renderer.h"
#else
#define surfaceBlocks visisbleCubes
#endif
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long draws, uploads, lookups;
#ifndef KERNELCRAFT_BASELINE
static int failUpload;
#endif
static PFNGLBUFFERSUBDATAPROC realBufferSubData;
static PFNGLBUFFERDATAPROC realBufferData;
static PFNGLGETUNIFORMLOCATIONPROC realGetUniformLocation;

void __real_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  draws++;
  __real_glDrawArrays(mode, first, count);
}
void __real_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
void __wrap_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
  draws++;
  __real_glDrawElements(mode, count, type, indices);
}
static void GLAPIENTRY countSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  uploads++;
  realBufferSubData(target, offset, size, data);
}
static void GLAPIENTRY countData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  uploads++;
#ifndef KERNELCRAFT_BASELINE
  if (failUpload && --failUpload == 0) {
    // Generate a real driver error on either the vertex or index upload.
    realBufferData(target, -1, data, usage);
    return;
  }
#endif
  realBufferData(target, size, data, usage);
}
static GLint GLAPIENTRY countLookup(GLuint program, const GLchar* name) {
  lookups++;
  return realGetUniformLocation(program, name);
}

int main(int argc, char** argv) {
  glutInit(&argc, argv);
  if (!glfwInit())
    return 1;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(960, 540, "render test", NULL, NULL);
  if (!window)
    return 1;
  glfwMakeContextCurrent(window);
  if (glewInit() != GLEW_OK)
    return 1;
  while (glGetError() != GL_NO_ERROR) {
  }
  glViewport(0, 0, 960, 540);
  glEnable(GL_DEPTH_TEST);
  printf("renderer: %s\n", glGetString(GL_RENDERER));
  GLuint shader = loadShaders("assets/shaders/vertex_shader.glsl", "assets/shaders/fragment_shader.glsl");
  if (!shader)
    return 1;
  double start = glfwGetTime();
#ifdef KERNELCRAFT_BASELINE
  initWorld();
  initCube();
  initChunks();
#else
  if (!initChunks() || !initWorld(shader))
    return 1;
#endif
  printf("initialization_ms: %.3f\n", (glfwGetTime() - start) * 1000.0);
  printf("block_storage_bytes: %zu\n", sizeof(Block) * (size_t)WORLD_SIZE * WORLD_SIZE * WORLD_HEIGHT);
  HUDInit("kernelcraft", "benchmark");
  realBufferSubData = __glewBufferSubData;
  realBufferData = __glewBufferData;
  realGetUniformLocation = __glewGetUniformLocation;
  __glewBufferSubData = countSubData;
  __glewBufferData = countData;
  __glewGetUniformLocation = countLookup;
  const float pitches[] = {0.0f, -30.0f, 89.0f, -45.0f};
  for (int scenario = 0; scenario < 4; scenario++) {
    Camera camera;
    initCamera(&camera);
    camera.pitch = pitches[scenario];
    if (scenario == 2)
      camera.position.y = 40.0f;
    if (scenario == 3)
      camera.position.y = 32.0f;
    updateCameraVectors(&camera);
    RenderResult result = {0};
    double elapsed = 0;
    for (int frame = -10; frame < 60; frame++) {
      if (frame == 0) {
        draws = uploads = lookups = 0;
        start = glfwGetTime();
      }
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      Mat4 view, projection;
      Vec3 target;
      vec3_add(&target, &camera.position, &camera.front);
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      mat4_perspective(projection, 70.0f, 960.0f / 540.0f, 0.1f, 1000.0f);
#ifdef KERNELCRAFT_BASELINE
      renderChunkGrid(shader, &camera);
      result = renderWorld(shader, &camera);
#else
      result = renderWorld(&camera, view, projection);
      if (!result.success || result.chunksRebuilt || result.chunksConsidered != CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)
        return 7;
#endif
      DebugData data = {&camera, 60.0f, result.surfaceBlocks};
      HUDDraw(shader, &data);
      glFinish();
    }
    elapsed = glfwGetTime() - start;
    printf("pitch=%5.1f frame_ms=%.3f draws_per_frame=%lu uploads_per_frame=%lu lookups_per_frame=%lu visible_blocks=%d\n", pitches[scenario], elapsed * 1000.0 / 60, draws / 60,
           uploads / 60, lookups / 60, result.surfaceBlocks);
#ifndef KERNELCRAFT_BASELINE
    if (uploads || lookups || draws > 60 * (4 * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS + 1))
      return 2;
    if (scenario == 2 && result.surfaceBlocks != 0)
      return 3;
    if (scenario != 2 && (result.surfaceBlocks == 0 || draws <= 60))
      return 3;
#endif
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
      fprintf(stderr, "GL error: %u\n", error);
      return 4;
    }
    {
      unsigned char* pixels = malloc(960 * 540 * 3);
      if (!pixels)
        return 1;
      glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
#ifndef KERNELCRAFT_BASELINE
      // The grid and HUD are grayscale. Colored pixels outside the HUD prove
      // that the terrain shader, textures, and mesh attributes produced output.
      int terrainPixels = 0;
      for (int y = 0; y < 540; y++)
        for (int x = 400; x < 960; x++) {
          const unsigned char* pixel = pixels + (y * 960 + x) * 3;
          if (abs(pixel[0] - pixel[1]) > 5 || abs(pixel[1] - pixel[2]) > 5)
            terrainPixels++;
        }
      if (scenario == 3 && terrainPixels < 500) {
        fprintf(stderr, "Exterior terrain did not produce visible textured pixels\n");
        free(pixels);
        return 5;
      }
#endif
      if (argc > 1) {
        char filename[512];
        snprintf(filename, sizeof(filename), "%s-%d.ppm", argv[1], scenario);
        FILE* capture = fopen(filename, "wb");
        if (!capture)
          return 1;
        fprintf(capture, "P6\n960 540\n255\n");
        for (int row = 539; row >= 0; row--)
          fwrite(pixels + row * 960 * 3, 1, 960 * 3, capture);
        fclose(capture);
      }
      free(pixels);
    }
  }
#ifndef KERNELCRAFT_BASELINE
  // Free flight must also render a block's surface from inside the block.
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i index = {x, z};
      Chunk* chunk = getChunk(&index);
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
    }
  Vec3i blockPosition = {0, 0, 0};
  setBlock(&blockPosition, BLOCK_STONE);
  if (!initWorld(shader))
    return 1;
  Camera inside = {.position = {0.5f, 0.5f, 0.5f}, .front = {0, 0, 1}, .up = {0, 1, 0}};
  Vec3 target = {0.5f, 0.5f, 1.5f};
  Mat4 view, projection;
  mat4_lookAt(view, &inside.position, &target, &inside.up);
  mat4_perspective(projection, 70, 960.0f / 540.0f, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult insideResult = renderWorld(&inside, view, projection);
  unsigned char centerPixel[4] = {0};
  glReadPixels(480, 270, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel);
  if (insideResult.surfaceBlocks != 1 || (centerPixel[0] == 0 && centerPixel[1] == 0 && centerPixel[2] == 0)) {
    fprintf(stderr, "The block surface disappeared during free flight inside terrain\n");
    return 6;
  }
  // Edits must update both sides of a chunk seam in the next frame.
  setBlock(&blockPosition, BLOCK_AIR);
  Vec3i left = {-1, 20, 1}, right = {0, 20, 1};
  setBlock(&left, BLOCK_STONE);
  setBlock(&right, BLOCK_STONE);
  Camera editCamera = {.position = {-3, 20.5f, 1.5f}, .front = {1, 0, 0}, .up = {0, 1, 0}};
  target = (Vec3){0, 20.5f, 1.5f};
  mat4_lookAt(view, &editCamera.position, &target, &editCamera.up);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult edited = renderWorld(&editCamera, view, projection);
  if (!edited.success || edited.submittedFaces != 10 || edited.chunksRebuilt != 3)
    return 7;
  uploads = 0;
  setBlock(&left, BLOCK_AIR);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  edited = renderWorld(&editCamera, view, projection);
  glReadPixels(480, 270, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel);
  if (!edited.success || edited.chunksRebuilt != 2 || edited.submittedFaces != 6 || uploads != 2 ||
      (centerPixel[0] == 0 && centerPixel[1] == 0 && centerPixel[2] == 0))
    return 8;
  uploads = 0;
  edited = renderWorld(&editCamera, view, projection);
  if (!edited.success || edited.chunksRebuilt || uploads)
    return 9;
  setBlock(&right, BLOCK_AIR);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  edited = renderWorld(&editCamera, view, projection);
  glReadPixels(480, 270, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel);
  if (!edited.success || edited.chunksRebuilt != 2 || edited.submittedFaces || edited.terrainDrawCalls ||
      centerPixel[0] || centerPixel[1] || centerPixel[2])
    return 10;
  // An upload can fail after an earlier buffer was already replaced. Do not
  // draw mismatched CPU/GPU state, clear the dirty flag, or lose cleanup handles.
  for (int failedBuffer = 1; failedBuffer <= 2; failedBuffer++) {
    setBlock(&right, BLOCK_STONE);
    failUpload = failedBuffer;
    draws = 0;
    edited = renderWorld(&editCamera, view, projection);
    if (edited.success || draws || !getChunk(&(Vec2i){8, 8})->dirty)
      return 11;
    cleanupWorld();
    if (glGetError() != GL_NO_ERROR || !initWorld(shader))
      return 12;
    setBlock(&right, BLOCK_AIR);
    if (!renderWorld(&editCamera, view, projection).success)
      return 13;
  }
  puts("Dirty mesh seam, removal, idle upload, framebuffer, and upload failure tests passed");
#endif
  __glewBufferSubData = realBufferSubData;
  __glewBufferData = realBufferData;
  __glewGetUniformLocation = realGetUniformLocation;
  cleanupWorld();
  cleanupChunks();
  glDeleteProgram(shader);
  if (glGetError() != GL_NO_ERROR)
    return 4;
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
