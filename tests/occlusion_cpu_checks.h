#include "world/occlusion.h"

static void test_software_occlusion(void) {
  OcclusionBuffer buffer;
  Mat4 identity;
  mat4_identity(identity);
  Vec3 wall[4] = {{-0.8f, -0.8f, 0}, {0.8f, -0.8f, 0}, {0.8f, 0.8f, 0}, {-0.8f, 0.8f, 0}};
  Vec3 min = {-0.1f, -0.1f, 0.5f}, max = {0.1f, 0.1f, 0.6f};
  occlusionClear(&buffer, identity, 1280, 720);
  CHECK(!occlusionBoundsHidden(&buffer, min, max));
  occlusionRasterizeQuad(&buffer, wall);
  CHECK(occlusionBoundsHidden(&buffer, min, max));
  // Equal or nearer depth, exposed edges, and a near-plane crossing survive.
  CHECK(!occlusionBoundsHidden(&buffer, (Vec3){-0.1f, -0.1f, -0.5f}, max));
  CHECK(!occlusionBoundsHidden(&buffer, (Vec3){-0.1f, -0.1f, 0}, max));
  CHECK(!occlusionBoundsHidden(&buffer, min, (Vec3){0.9f, 0.1f, 0.6f}));
  CHECK(!occlusionBoundsHidden(&buffer, (Vec3){-0.1f, -0.1f, -1.1f}, max));
  // Reversed winding must work: debug flight renders both sides of terrain.
  Vec3 reversed[4] = {wall[3], wall[2], wall[1], wall[0]};
  occlusionClear(&buffer, identity, 1280, 720);
  occlusionRasterizeQuad(&buffer, reversed);
  CHECK(occlusionBoundsHidden(&buffer, min, max));
  // A fraction-of-a-cell slit remains open even though its center is covered.
  Vec3 left[4] = {{-1, -1, 0}, {-0.0001f, -1, 0}, {-0.0001f, 1, 0}, {-1, 1, 0}};
  Vec3 right[4] = {{0.0001f, -1, 0}, {1, -1, 0}, {1, 1, 0}, {0.0001f, 1, 0}};
  occlusionClear(&buffer, identity, 1280, 720);
  occlusionRasterizeQuad(&buffer, left);
  occlusionRasterizeQuad(&buffer, right);
  CHECK(!occlusionBoundsHidden(&buffer, min, max));
  // A slanted occluder must use its farthest depth, not its nearest corner.
  wall[0].z = wall[3].z = -0.5f;
  wall[1].z = wall[2].z = 0.8f;
  occlusionClear(&buffer, identity, 1280, 720);
  occlusionRasterizeQuad(&buffer, wall);
  CHECK(!occlusionBoundsHidden(&buffer, min, max));
  // Changed cameras and invalid projections cannot retain old coverage.
  occlusionClear(&buffer, identity, 1280, 720);
  CHECK(!occlusionBoundsHidden(&buffer, min, max));
  identity[0] = NAN;
  occlusionClear(&buffer, identity, 1280, 720);
  occlusionRasterizeQuad(&buffer, wall);
  CHECK(!occlusionBoundsHidden(&buffer, min, max));

  Mat4 projection;
  mat4_perspective(projection, 70, 16.0f / 9, 0.1f, 1000);
  Vec3 perspectiveWall[4] = {{-10, -10, -5}, {10, -10, -5}, {10, 10, -5}, {-10, 10, -5}};
  occlusionClear(&buffer, projection, 1280, 720);
  occlusionRasterizeQuad(&buffer, perspectiveWall);
  CHECK(occlusionBoundsHidden(&buffer, (Vec3){-1, -1, -12}, (Vec3){1, 1, -10}));
  CHECK(!occlusionBoundsHidden(&buffer, (Vec3){-1, -1, -12}, (Vec3){1, 1, 0}));
  // A rectangle crossing the near plane is discarded, never projected behind us.
  perspectiveWall[0].z = 1;
  occlusionClear(&buffer, projection, 1280, 720);
  occlusionRasterizeQuad(&buffer, perspectiveWall);
  CHECK(!occlusionBoundsHidden(&buffer, (Vec3){-1, -1, -12}, (Vec3){1, 1, -10}));
  puts("Conservative software depth coverage, gaps, winding, projection, and invalidation checks passed");
}

static void check_occluder_mesh(const ChunkMesh* mesh) {
  MeshOccluders occluders;
  buildMeshOccluders(mesh, &occluders);
  CHECK(occluders.count >= 0 && occluders.count <= OCCLUSION_QUADS);
  for (int i = 0; i < occluders.count; i++) {
    CHECK(occluders.quads[i].area >= 4 * CUBE_SIZE * CUBE_SIZE);
    if (i)
      CHECK(occluders.quads[i - 1].area >= occluders.quads[i].area);
    bool found = false;
    for (size_t first = 0; first + 3 < mesh->vertexCount; first += 4) {
      bool same = true;
      for (int corner = 0; corner < 4; corner++)
        same &= memcmp(&mesh->vertices[first + corner].position, &occluders.quads[i].corners[corner], sizeof(Vec3)) == 0;
      found |= same;
    }

    CHECK(found); // A chunk box or an invented solid span cannot become an occluder.
  }
}

#include "world/mesh_visibility.h"

static void test_mesh_visibility(void) {
  // An orthographic box centered at negative world coordinates.
  const float box[6][4] = {{1, 0, 0, 11}, {-1, 0, 0, -9}, {0, 1, 0, 1}, {0, -1, 0, 1}, {0, 0, 1, 6}, {0, 0, -1, -4}};
  ChunkMesh mesh = {0};
  MeshVisibility visibility;
  CHECK(buildMeshVisibility(&mesh, &visibility));
  CHECK(!meshVisibilityIntersects(&visibility, box));
  freeMeshVisibility(&visibility);
  freeMeshVisibility(&visibility);

  MeshVertex vertices[8] = {0};
  mesh.vertices = vertices;
  mesh.vertexCount = 4;
  const Vec3 rectangles[][4] = {
      {{-10.5f, -0.5f, -5}, {-9.5f, -0.5f, -5}, {-9.5f, 0.5f, -5}, {-10.5f, 0.5f, -5}}, // Inside.
      {{-12, -2, -5}, {-8, -2, -5}, {-8, 2, -5}, {-12, 2, -5}},                         // All corners outside, but covers the view.
      {{-10, -0.5f, -5}, {-10, 0.5f, -5}, {-10, 0.5f, -3}, {-10, -0.5f, -3}},           // Crosses near plane.
      {{-12, 0, -5}, {-8, 0, -5}, {-8, 0, -3}, {-12, 0, -3}},                           // Horizontal, crosses near plane.
      {{-10.5f, -0.5f, -4}, {-9.5f, -0.5f, -4}, {-9.5f, 0.5f, -4}, {-10.5f, 0.5f, -4}}, // Near-plane contact.
      {{-10.5f, -0.5f, -3}, {-9.5f, -0.5f, -3}, {-9.5f, 0.5f, -3}, {-10.5f, 0.5f, -3}}, // Before near plane.
      {{-10.5f, -0.5f, -7}, {-9.5f, -0.5f, -7}, {-9.5f, 0.5f, -7}, {-10.5f, 0.5f, -7}}, // Beyond far plane.
  };

  for (size_t rectangle = 0; rectangle < sizeof(rectangles) / sizeof(rectangles[0]); rectangle++) {
    for (int i = 0; i < 4; i++)
      vertices[i].position = rectangles[rectangle][i];
    CHECK(buildMeshVisibility(&mesh, &visibility));
    CHECK(meshVisibilityIntersects(&visibility, box) == (rectangle < 5));
    freeMeshVisibility(&visibility);
  }

  // A broad mesh box intersects the view, but both separated surfaces miss it.
  mesh.vertexCount = 8;
  for (int i = 0; i < 8; i++) {
    vertices[i].position = rectangles[0][i % 4];
    vertices[i].position.x += i < 4 ? -5 : 5;
  }

  CHECK(buildMeshVisibility(&mesh, &visibility));
  CHECK(visibility.surfaceCount == 2);
  CHECK(!meshVisibilityIntersects(&visibility, box));
  freeMeshVisibility(&visibility);

  // A rectangle beyond a rotated frustum corner passes all six individual
  // box/plane tests, but clipping must reject it.
  const float corner[6][4] = {{-0.70710678f, -0.70710678f, 0, 0.70710678f},
                              {0.70710678f, 0.70710678f, 0, 0.70710678f},
                              {-0.70710678f, 0.70710678f, 0, 0.70710678f},
                              {0.70710678f, -0.70710678f, 0, 0.70710678f},
                              {0, 0, 1, 1},
                              {0, 0, -1, 1}};
  mesh.vertexCount = 4;
  vertices[0].position = (Vec3){1.5f, -1, 0};
  vertices[1].position = (Vec3){1.6f, -1, 0};
  vertices[2].position = (Vec3){1.6f, 1, 0};
  vertices[3].position = (Vec3){1.5f, 1, 0};
  CHECK(buildMeshVisibility(&mesh, &visibility));
  CHECK(!meshVisibilityIntersects(&visibility, corner));
  freeMeshVisibility(&visibility);
}
