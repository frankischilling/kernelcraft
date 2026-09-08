#include <GL/glew.h>
#include "graphics/camera.h"
#include "graphics/hud.h"
#include "graphics/shader.h"
#include "world/world.h"
#ifndef KERNELCRAFT_BASELINE
#include "graphics/world_renderer.h"
#include "graphics/selection.h"
#include "world/edit.h"
#include "graphics/texture.h"
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
    printf("Injecting buffer upload failure (target %u)\n", target);
    // Invalid usage generates a GL error without requesting an allocation.
    // A negative index-buffer size crashed Intel driver 32.0.101.7077.
    realBufferData(target, size, data, GL_NONE);
    return;
  }
#endif
  realBufferData(target, size, data, usage);
}
static GLint GLAPIENTRY countLookup(GLuint program, const GLchar* name) {
  lookups++;
  return realGetUniformLocation(program, name);
}

#ifndef KERNELCRAFT_BASELINE
#include "terrain_render_checks.h"
// Independent sampler2D reference for the original Phong shader by
// frankischilling (2024-11-20). Keep this separate from the array shader so
// incorrect layer selection cannot change both sides of the pixel comparison.
static GLuint referenceProgram(void) {
  const char* path = "test-material-reference.frag";
  const char* source = "#version 330 core\n"
                       "in vec3 FragPos; in vec3 Normal; in vec2 TexCoord; out vec4 FragColor;\n"
                       "uniform vec3 lightPos, viewPos, lightColor; uniform sampler2D texture1;\n"
                       "void main(){\n"
                       "vec3 norm=normalize(Normal); vec3 lightDir=normalize(lightPos-FragPos);\n"
                       "vec3 ambient=0.2*lightColor; vec3 diffuse=max(dot(norm,lightDir),0.0)*lightColor;\n"
                       "vec3 viewDir=normalize(viewPos-FragPos); vec3 reflectDir=reflect(-lightDir,norm);\n"
                       "float spec=pow(max(dot(viewDir,reflectDir),0.0),32); vec3 specular=0.5*spec*lightColor;\n"
                       "FragColor=vec4((ambient+diffuse+specular)*texture(texture1,TexCoord).rgb,1.0);}\n";
  FILE* file = fopen(path, "wb");
  if (!file)
    return 0;
  bool written = fputs(source, file) >= 0;
  if (fclose(file) != 0)
    written = false;
  GLuint program = written ? loadShaders("assets/shaders/vertex_shader.glsl", path) : 0;
  remove(path);
  return program;
}

/* Compare the running renderer with independent unit-cube submissions. Six
 * views exercise every face of grass, dirt, stone, and mixed-material prisms. */
static bool testRepeatedTextures(GLuint shader, int pattern) {
  const int materials[] = {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE};
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
    }
  for (int x = 1; x < 5; x++)
    for (int y = 20; y < 23; y++)
      for (int z = 1; z < 4; z++)
        setBlock(&(Vec3i){x, y, z}, materials[pattern < 3 ? pattern : (x + y + z) % 3]);
  if (!initWorld(shader))
    return false;
  GLuint textures[] = {loadTexture("assets/textures/stone.png"),      loadTexture("assets/textures/dirt.png"),       loadTexture("assets/textures/grass-top.png"),
                       loadTexture("assets/textures/grass-side.png"), loadTexture("assets/textures/dirt-rocks.png"), loadTexture("assets/textures/grass-top-leaves.png"),
                       loadTexture("assets/textures/grass-bug.png")};
  GLuint referenceShader = referenceProgram();
  GLuint vao = 0, vbo = 0;
  size_t bytes = 960 * 540 * 3;
  unsigned char* merged = malloc(bytes);
  unsigned char* reference = malloc(bytes);
  bool success = merged && reference && referenceShader;
  for (int layer = 0; layer < 7; layer++)
    success &= textures[layer] != 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  for (int attribute = 0; attribute < 3; attribute++)
    glEnableVertexAttribArray(attribute);
  for (int direction = 0; success && direction < 6; direction++) {
    Vec3i normal = vec3iFaceMap[direction];
    Vec3 center = {3, 21.5f, 2.5f};
    Camera camera = {.position = {center.x + normal.x * 7, center.y + normal.y * 7, center.z + normal.z * 7}, .up = {0, normal.y ? 0 : 1, normal.y ? 1 : 0}};
    vec3_subtract(&camera.front, &center, &camera.position);
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &center, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540.0f, 0.1f, 1000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult result = renderWorld(&camera, view, projection);
    if (!result.success || (pattern < 3 && result.submittedQuads != 6) || result.terrainDrawCalls != 1) {
      success = false;
      break;
    }
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, merged);
    // Use the original 2D sampler with independent face material selection.
    // The grid is outside the compared prism pixels.
    Mat4 viewProjection;
    mat4_multiply(viewProjection, projection, view);
    glUseProgram(referenceShader);
    glUniformMatrix4fv(glGetUniformLocation(referenceShader, "viewProjection"), 1, GL_FALSE, viewProjection);
    glUniform3f(glGetUniformLocation(referenceShader, "viewPos"), camera.position.x, camera.position.y, camera.position.z);
    glUniform3f(glGetUniformLocation(referenceShader, "lightPos"), 5, 50, 5);
    glUniform3f(glGetUniformLocation(referenceShader, "lightColor"), 1, 1, 1);
    glUniform1i(glGetUniformLocation(referenceShader, "texture1"), 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int x = 1; x < 5; x++)
      for (int y = 20; y < 23; y++)
        for (int z = 1; z < 4; z++)
          for (int face = 0; face < 6; face++) {
            float vertices[48];
            memcpy(vertices, getCubeFaceVertices(face), sizeof(vertices));
            for (int corner = 0; corner < 6; corner++) {
              vertices[corner * 8] = (vertices[corner * 8] + x + 0.5f) * CUBE_SIZE;
              vertices[corner * 8 + 1] = (vertices[corner * 8 + 1] + y + 0.5f) * CUBE_SIZE;
              vertices[corner * 8 + 2] = (vertices[corner * 8 + 2] + z + 0.5f) * CUBE_SIZE;
            }
            int id = materials[pattern < 3 ? pattern : (x + y + z) % 3];
            int material = id == BLOCK_STONE ? 0 : id == BLOCK_DIRT || face == BOTTOM ? 1 : face == TOP ? 2 : 3;
            material = referenceTerrainLayer(material, (Vec3i){x, y, z}, worldSeed());
            glBindTexture(GL_TEXTURE_2D, textures[material]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
          }
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, reference);
    size_t compared = 0, different = 0;
    for (size_t pixel = 0; pixel < bytes; pixel += 3) {
      if (!reference[pixel] && !reference[pixel + 1] && !reference[pixel + 2])
        continue;
      compared++;
      for (int channel = 0; channel < 3; channel++)
        if (abs(reference[pixel + channel] - merged[pixel + channel]) > 3) {
          different++;
          break;
        }
    }
    /* Nearest sampling can differ at texel boundaries after retriangulation.
     * Permit at most 0.2% differing pixels, not stretched or rotated tiles. */
    printf("Repeated texture pattern %d face %d: %zu/%zu differing pixels\n", pattern, direction, different, compared);
    success = compared > 10000 && different * 500 <= compared && glGetError() == GL_NO_ERROR;
  }
  free(merged);
  free(reference);
  glBindVertexArray(0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteTextures(7, textures);
  glUseProgram(0);
  glDeleteProgram(referenceShader);
  return success;
}

static bool selectionPixelNear(const unsigned char* pixels, const Mat4 matrix, Vec3 point) {
  float w = matrix[3] * point.x + matrix[7] * point.y + matrix[11] * point.z + matrix[15];
  float x = matrix[0] * point.x + matrix[4] * point.y + matrix[8] * point.z + matrix[12];
  float y = matrix[1] * point.x + matrix[5] * point.y + matrix[9] * point.z + matrix[13];
  if (w <= 0)
    return false;
  int screenX = (int)((x / w + 1) * 480), screenY = (int)((y / w + 1) * 270);
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      int px = screenX + dx, py = screenY + dy;
      if (px < 0 || px >= 960 || py < 0 || py >= 540)
        continue;
      const unsigned char* color = pixels + (py * 960 + px) * 3;
      if (color[0] > 240 && color[1] > 180 && color[2] < 100)
        return true;
    }
  return false;
}

static bool testSelectionVisibility(GLuint shader) {
  const Vec3i selected = {-1, 20, 0};
  const float angles[] = {0, 30, -30, 60, -60, 75, -75, 82, -82};
  unsigned char* pixels = malloc(960 * 540 * 3);
  if (!pixels)
    return false;
  bool success = true;
  for (int face = 0; face < 6; face++) {
    for (int x = 0; x < CHUNKS_PER_AXIS; x++)
      for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
        Chunk* chunk = getChunk(&(Vec2i){x, z});
        memset(chunk->blocks, 0, sizeof(chunk->blocks));
      }
    Vec3 normal = vec3FaceMap[face], u = normal.x ? (Vec3){0, 0, 1} : (Vec3){1, 0, 0}, v;
    vec3_cross(&v, &normal, &u);
    // The selected face is in the middle of a flush 3x3 surface.
    for (int a = -1; a <= 1; a++)
      for (int b = -1; b <= 1; b++)
        setBlock(&(Vec3i){selected.x + (int)(a * u.x + b * v.x), selected.y + (int)(a * u.y + b * v.y), selected.z + (int)(a * u.z + b * v.z)}, BLOCK_STONE);
    if (!initWorld(shader)) {
      success = false;
      break;
    }
    Vec3 center = {selected.x + 0.5f + normal.x * 0.5f, selected.y + 0.5f + normal.y * 0.5f, selected.z + 0.5f + normal.z * 0.5f};
    for (size_t angle = 0; angle < sizeof(angles) / sizeof(angles[0]); angle++) {
      float outward = 4 * cosf(toRadians(angles[angle])), tangent = 4 * sinf(toRadians(angles[angle]));
      Camera camera = {.position = {center.x + normal.x * outward + (v.x + u.x) * tangent * 0.70710678f, center.y + normal.y * outward + (v.y + u.y) * tangent * 0.70710678f,
                                    center.z + normal.z * outward + (v.z + u.z) * tangent * 0.70710678f},
                       .up = v};
      vec3_subtract(&camera.front, &center, &camera.position);
      Mat4 view, projection, combined;
      mat4_lookAt(view, &camera.position, &center, &camera.up);
      mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
      mat4_multiply(combined, projection, view);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      RenderResult result = renderWorld(&camera, view, projection);
      Ray hit = rayCast(camera.position, camera.front, EDIT_REACH);
      if (!result.success || !hit.hit || hit.blockCoords.x != selected.x || hit.blockCoords.y != selected.y || hit.blockCoords.z != selected.z) {
        fprintf(stderr, "Selection fixture missed face %d at angle %.0f\n", face, angles[angle]);
        success = false;
        continue;
      }
      drawSelection(&hit, view, projection);
      glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      for (int edge = 0; edge < 4; edge++) {
        int visible = 0;
        for (int sample = 0; sample < 32; sample++) {
          float along = -0.4f + 0.8f * sample / 31;
          float a = edge < 2 ? along : (edge == 2 ? -0.5f : 0.5f);
          float b = edge < 2 ? (edge == 0 ? -0.5f : 0.5f) : along;
          Vec3 point = {center.x + a * u.x + b * v.x, center.y + a * u.y + b * v.y, center.z + a * u.z + b * v.z};
          visible += selectionPixelNear(pixels, combined, point);
        }
        if (visible < 29) {
          fprintf(stderr, "Selection face %d angle %.0f edge %d: only %d/32 visible samples\n", face, angles[angle], edge, visible);
          success = false;
        }
      }
    }
  }
  free(pixels);
  return success;
}

static bool testNeighborSelection(GLuint shader, const char* capturePrefix) {
  const Vec3i selected = {-1, 20, 0};
  unsigned char* pixels = malloc(960 * 540 * 3);
  if (!pixels)
    return false;
  bool success = true;
  for (int layout = 0; layout < 2; layout++)
    for (int face = 0; face < 6; face++) {
      for (int x = 0; x < CHUNKS_PER_AXIS; x++)
        for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
          Chunk* chunk = getChunk(&(Vec2i){x, z});
          memset(chunk->blocks, 0, sizeof(chunk->blocks));
        }
      Vec3 n = vec3FaceMap[face], u = n.x ? (Vec3){0, 0, 1} : (Vec3){1, 0, 0}, v;
      vec3_cross(&v, &n, &u);
      // Rotate the same floor/neighbor arrangement onto all six faces.
      // Layout 0 matches a block on the ground beside a flush neighbor.
      // Layout 1 also extends that neighbor toward the camera as a side wall.
      for (int a = -2; a <= 2; a++)
        for (int b = -1; b <= 2; b++)
          for (int c = -2; c <= 3; c++) {
            bool floor = b == -1;
            bool neighbor = a == 1 && (layout ? b >= 0 && c >= -1 : b == 0 && c == 0);
            if (floor || neighbor || (a == 0 && b == 0 && c == 0))
              setBlock(&(Vec3i){selected.x + (int)(a * u.x + b * v.x + c * n.x), selected.y + (int)(a * u.y + b * v.y + c * n.y), selected.z + (int)(a * u.z + b * v.z + c * n.z)},
                       BLOCK_GRASS);
          }
      if (!initWorld(shader)) {
        free(pixels);
        return false;
      }
      Vec3 center = {selected.x + 0.5f + n.x * 0.5f, selected.y + 0.5f + n.y * 0.5f, selected.z + 0.5f + n.z * 0.5f};
      for (int angle = 0; angle < 3; angle++) {
        Camera camera = {.position = {center.x + 3.5f * n.x + (0.75f + angle * 0.75f) * v.x - angle * u.x, center.y + 3.5f * n.y + (0.75f + angle * 0.75f) * v.y - angle * u.y,
                                      center.z + 3.5f * n.z + (0.75f + angle * 0.75f) * v.z - angle * u.z},
                         .up = v};
        vec3_subtract(&camera.front, &center, &camera.position);
        Mat4 view, projection, combined;
        mat4_lookAt(view, &camera.position, &center, &camera.up);
        mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
        mat4_multiply(combined, projection, view);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderResult result = renderWorld(&camera, view, projection);
        Ray hit = rayCast(camera.position, camera.front, EDIT_REACH);
        if (!result.success || !hit.hit || hit.blockCoords.x != selected.x || hit.blockCoords.y != selected.y || hit.blockCoords.z != selected.z) {
          fprintf(stderr, "Neighbor selection fixture missed layout %d face %d view %d\n", layout, face, angle);
          success = false;
          continue;
        }
        drawSelection(&hit, view, projection);
        glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        if (capturePrefix && layout == 0 && face == FRONT && angle == 0) {
          char filename[512];
          snprintf(filename, sizeof(filename), "%s-selection-neighbors.ppm", capturePrefix);
          FILE* capture = fopen(filename, "wb");
          if (!capture) {
            free(pixels);
            return false;
          }
          fprintf(capture, "P6\n960 540\n255\n");
          for (int row = 539; row >= 0; row--)
            fwrite(pixels + row * 960 * 3, 1, 960 * 3, capture);
          fclose(capture);
        }
        // Four edges of the aimed face, plus the other three top edges in
        // the reference arrangement. Every sampled boundary is visible terrain.
        for (int edge = 0; edge < (layout ? 4 : 7); edge++) {
          int visible = 0;
          for (int sample = 0; sample < 32; sample++) {
            float along = -0.45f + 0.9f * sample / 31;
            float a = edge < 2 || edge == 4 ? along : edge == 2 || edge == 5 ? -0.5f : 0.5f;
            float b = edge < 2 ? (edge == 0 ? -0.5f : 0.5f) : edge < 4 ? along : 0.5f;
            float c = edge < 4 ? 0 : edge == 4 ? -1 : along - 0.5f;
            Vec3 point = {center.x + a * u.x + b * v.x + c * n.x, center.y + a * u.y + b * v.y + c * n.y, center.z + a * u.z + b * v.z + c * n.z};
            visible += selectionPixelNear(pixels, combined, point);
          }
          if (visible < 30) {
            fprintf(stderr, "Neighbor selection layout %d face %d view %d edge %d: only %d/32 visible samples\n", layout, face, angle, edge, visible);
            success = false;
          }
        }
      }
    }
  free(pixels);
  if (success)
    puts("Selection floor and side boundaries passed in 36 views");
  return success;
}

static bool testCloseSelection(GLuint shader) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
    }
  setBlock(&(Vec3i){-1, 20, 0}, BLOCK_STONE);
  if (!initWorld(shader))
    return false;
  const Vec3 normals[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (int face = 0; face < 6; face++) {
    Vec3 n = normals[face], center = {-0.5f + n.x * 0.5f, 20.5f + n.y * 0.5f, 0.5f + n.z * 0.5f};
    // A two-block-high passage leaves 0.38 blocks between the eye and ceiling.
    // At this distance each face fills the screen, with all outline edges clipped.
    Camera camera = {.position = {center.x + n.x * 0.38f, center.y + n.y * 0.38f, center.z + n.z * 0.38f}, .front = {-n.x, -n.y, -n.z}, .up = {0, n.y ? 0 : 1, n.y ? 1 : 0}};
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &center, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult result = renderWorld(&camera, view, projection);
    Ray hit = rayCast(camera.position, camera.front, EDIT_REACH);
    unsigned char before[64 * 64 * 3], after[sizeof(before)];
    glReadPixels(448, 238, 64, 64, GL_RGB, GL_UNSIGNED_BYTE, before);
    drawSelection(&hit, view, projection);
    glReadPixels(448, 238, 64, 64, GL_RGB, GL_UNSIGNED_BYTE, after);
    int highlighted = 0;
    for (size_t pixel = 0; pixel < sizeof(before); pixel += 3)
      highlighted += abs(after[pixel] - before[pixel]) > 5 || abs(after[pixel + 1] - before[pixel + 1]) > 5 || abs(after[pixel + 2] - before[pixel + 2]) > 5;
    printf("Close selection face %d: %d/4096 highlighted pixels\n", face, highlighted);
    if (!result.success || !hit.hit || hit.normal.x != n.x || hit.normal.y != n.y || hit.normal.z != n.z || highlighted <= 4000 || glGetError() != GL_NO_ERROR)
      return false;
  }
  return true;
}

static bool testSelectionOcclusionAndState(GLuint shader) {
  // testCloseSelection left the isolated selected block at (-1,20,0).
  setBlock(&(Vec3i){0, 20, 0}, BLOCK_STONE);
  Camera camera = {.position = {-0.5f, 20.5f, 4.5f}, .front = {0, 0, -1}, .up = {0, 1, 0}};
  Vec3 target = {-0.5f, 20.5f, 1};
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(&camera, view, projection).success)
    return false;
  Ray hit = rayCast(camera.position, camera.front, EDIT_REACH);
  if (!hit.hit || hit.blockCoords.x != -1 || hit.normal.z != 1)
    return false;
  unsigned char neighborBefore[16 * 10 * 3], neighborAfter[sizeof(neighborBefore)];
  float depthBefore[64 * 64], depthAfter[64 * 64];
  glReadPixels(580, 265, 16, 10, GL_RGB, GL_UNSIGNED_BYTE, neighborBefore);
  glReadPixels(448, 238, 64, 64, GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore);
  // Deliberately differ from the overlay's setup, then verify the caller's state.
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
  glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(3, 7);
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glColor4f(0.2f, 0.4f, 0.6f, 0.8f);
  glLineWidth(1);
  glMatrixMode(GL_TEXTURE);
  drawSelection(&hit, view, projection);
  const GLenum names[] = {GL_CURRENT_PROGRAM, GL_MATRIX_MODE,     GL_DEPTH_FUNC,         GL_DEPTH_WRITEMASK,      GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB,
                          GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_CULL_FACE_MODE};
  const GLint expected[] = {(GLint)shader,    GL_TEXTURE, GL_GREATER, GL_TRUE, GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_FUNC_REVERSE_SUBTRACT,
                            GL_FUNC_SUBTRACT, GL_FRONT};
  bool restored = !glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && glIsEnabled(GL_POLYGON_OFFSET_FILL) && !glIsEnabled(GL_POLYGON_OFFSET_LINE);
  for (size_t state = 0; state < sizeof(names) / sizeof(names[0]); state++) {
    GLint value;
    glGetIntegerv(names[state], &value);
    restored &= value == expected[state];
  }
  GLint polygonMode[2];
  GLfloat factor, units, color[4], width;
  glGetIntegerv(GL_POLYGON_MODE, polygonMode);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
  glGetFloatv(GL_CURRENT_COLOR, color);
  glGetFloatv(GL_LINE_WIDTH, &width);
  restored &= polygonMode[0] == GL_LINE && polygonMode[1] == GL_LINE && factor == 3 && units == 7 && width == 1 && color[0] == 0.2f && color[1] == 0.4f && color[2] == 0.6f &&
              color[3] == 0.8f;
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glMatrixMode(GL_MODELVIEW);
  glReadPixels(580, 265, 16, 10, GL_RGB, GL_UNSIGNED_BYTE, neighborAfter);
  glReadPixels(448, 238, 64, 64, GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
  if (!restored || memcmp(neighborBefore, neighborAfter, sizeof(neighborBefore)) || memcmp(depthBefore, depthAfter, sizeof(depthBefore))) {
    fprintf(stderr, "Selection changed caller state, neighboring pixels, or depth storage\n");
    return false;
  }
  // Even a stale selection must not draw through a nearer voxel.
  setBlock(&(Vec3i){-1, 20, 2}, BLOCK_STONE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(&camera, view, projection).success)
    return false;
  size_t bytes = 960 * 540 * 3;
  unsigned char* before = malloc(bytes);
  unsigned char* after = malloc(bytes);
  bool occluded = before && after;
  if (occluded) {
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, before);
    drawSelection(&hit, view, projection);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, after);
    occluded = memcmp(before, after, bytes) == 0;
    Ray miss = {0};
    drawSelection(&miss, view, projection);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, after);
    occluded &= memcmp(before, after, bytes) == 0;
    // Put the selected outline fully inside a taller foreground silhouette.
    // Near-coplanar top/side views must not bias hidden lines through this wall.
    for (int x = -2; x <= 0; x++)
      for (int y = 19; y <= 21; y++)
        setBlock(&(Vec3i){x, y, 2}, BLOCK_STONE);
    const Vec3 grazingEyes[] = {{-0.5f, 20.999f, 4.5f}, {-0.5f, 21.001f, 4.5f}, {-0.5f, 21.01f, 4.5f}, {-0.001f, 20.5f, 4.5f}, {0.001f, 20.5f, 4.5f}};
    for (size_t eye = 0; eye < sizeof(grazingEyes) / sizeof(grazingEyes[0]); eye++) {
      camera.position = grazingEyes[eye];
      vec3_subtract(&camera.front, &target, &camera.position);
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      if (!renderWorld(&camera, view, projection).success) {
        occluded = false;
        break;
      }
      glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, before);
      drawSelection(&hit, view, projection);
      glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, after);
      if (memcmp(before, after, bytes)) {
        fprintf(stderr, "Selection leaked through foreground at grazing view %zu\n", eye);
        occluded = false;
      }
    }
  }
  free(before);
  free(after);
  if (!occluded || glGetError() != GL_NO_ERROR) {
    fprintf(stderr, "Selection leaked through foreground terrain or drew a missed target\n");
    return false;
  }
  puts("Selection close-up, neighboring face, foreground occlusion, and GL state tests passed");
  return true;
}
#endif

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
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
#ifndef KERNELCRAFT_BASELINE
      data.selection = rayCast(camera.position, camera.front, EDIT_REACH);
      data.selectedBlock = BLOCK_GRASS;
      data.captured = true;
      data.stats = &result;
      data.showDebug = true;
      drawSelection(&data.selection, view, projection);
#endif
      HUDDraw(shader, &data);
      glFinish();
    }
    elapsed = glfwGetTime() - start;
    printf("pitch=%5.1f frame_ms=%.3f terrain_grid_draws_per_frame=%lu uploads_per_frame=%lu lookups_per_frame=%lu surface_blocks=%d\n", pitches[scenario], elapsed * 1000.0 / 60,
           draws / 60, uploads / 60, lookups / 60, result.surfaceBlocks);
#ifndef KERNELCRAFT_BASELINE
    printf("submitted_quads=%zu submitted_triangles=%zu chunks_rendered=%d\n", result.submittedQuads, result.submittedTriangles, result.chunksRendered);
    if (result.terrainDrawCalls != result.chunksRendered || draws != 60 * (unsigned long)(result.chunksRendered + 1)) {
      fprintf(stderr, "Expected one terrain draw per visible chunk plus the grid\n");
      return 15;
    }
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
  if (!edited.success || edited.submittedQuads != 10 || edited.chunksRebuilt != 3)
    return 7;
  uploads = 0;
  setBlock(&left, BLOCK_AIR);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  edited = renderWorld(&editCamera, view, projection);
  glReadPixels(480, 270, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel);
  if (!edited.success || edited.chunksRebuilt != 2 || edited.submittedQuads != 6 || uploads != 2 || (centerPixel[0] == 0 && centerPixel[1] == 0 && centerPixel[2] == 0))
    return 8;
  uploads = 0;
  edited = renderWorld(&editCamera, view, projection);
  if (!edited.success || edited.chunksRebuilt || uploads)
    return 9;
  setBlock(&right, BLOCK_AIR);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  edited = renderWorld(&editCamera, view, projection);
  glReadPixels(480, 270, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel);
  if (!edited.success || edited.chunksRebuilt != 2 || edited.submittedQuads || edited.terrainDrawCalls || centerPixel[0] || centerPixel[1] || centerPixel[2])
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
  if (!testFarTerrain(shader))
    return 20;
  if (!testTerrainVariants(shader))
    return 21;
  for (int pattern = 0; pattern < 4; pattern++)
    if (!testRepeatedTextures(shader, pattern)) {
      fprintf(stderr, "Merged textures differ from unit-cube rendering\n");
      return 14;
    }
  if (!testSelectionVisibility(shader))
    return 16;
  if (!testNeighborSelection(shader, argc > 1 ? argv[1] : NULL))
    return 19;
  if (!testCloseSelection(shader))
    return 17;
  if (!testSelectionOcclusionAndState(shader))
    return 18;
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
