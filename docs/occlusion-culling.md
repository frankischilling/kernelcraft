# Chunk occlusion culling

The solid renderer skips chunk meshes whose GPU visibility query found no
samples passing the depth test. This runs after the existing render-distance
and frustum filters. It does not change the world, greedy meshes, materials,
selection, movement, or save format.

The frustum filter checks actual mesh surfaces after the broad chunk box. A
CPU hierarchy of greedy-rectangle bounds rejects empty space inside that box;
leaf rectangles are clipped against the current frustum when needed. This
keeps sky-only views at zero terrain draws and zero queries during mouse look
and movement. A chunk box alone could accept those views, causing useless
draws whenever movement invalidated a previous zero-sample query.

The hierarchy is built with the mesh, replaced on dirty-chunk uploads, and
freed on cleanup. Traversal uses no per-frame allocation. Near-plane crossings
and boundary contact remain visible, and edits can reveal overhead blocks on
the next frame. This is a more precise frustum test; it does not reuse old
occlusion answers after movement or cull individual quads from a visible chunk.

Candidates are sorted by distance to their bounding boxes, nearest first, so
near opaque surfaces can hide farther chunks. A `GL_ANY_SAMPLES_PASSED` query
wraps each unknown chunk's normal terrain draw. There is no extra depth pass
or bounding-box draw. Results are read only after
`GL_QUERY_RESULT_AVAILABLE` reports completion; the renderer never waits for
a query, following the [Khronos query-result contract](https://wikis.khronos.org/opengl/GLAPI/glGetQueryObject).
Unknown or pending chunks remain drawable. Query objects belong to
the chunk's GPU resources and are deleted while the context is current.

A zero-sample result is valid only for the same view and opaque world. Every
camera-position, view-matrix, projection, viewport, framebuffer-binding, or
mesh change invalidates all cached visibility. Pending answers from before a
change are discarded before reuse. Wireframe bypasses queries and occlusion,
and returning to solid rendering starts with unknown visibility.

This conservative policy avoids delayed reveals after movement or block edits.
It also limits the optimization: continuous walking, mouse look, or a changing
running FOV draws all frustum candidates while fresh queries are pending.
Occlusion savings start when the view settles. Chunks partly exposed through
a hole remain drawable. A visible chunk is still submitted as one complete
mesh; this does not cull individual quads within it.

The renderer requires a cleared depth buffer each frame and the game's normal
opaque, depth-tested, depth-writing state. Replacing the world requires
`initWorld`; edits use the existing dirty-chunk path. Future transparency,
external occluders, or changes to depth/rasterization state need a separate
visibility policy before reusing this cache.

F3 reports `Hidden` chunks skipped by occlusion and new `Queries` alongside
terrain draws. Submitted quads/triangles exclude hidden chunks. Queries fall
to zero once a stationary view's answers are known. Mesh update time still
measures rebuilding/uploading meshes and does not include query polling.

## Regression coverage

`tests/occlusion_render_checks.h` runs inside the existing real OpenGL
benchmark, reached by `make test-gl`, `make benchmark`, and native
`.\build.cmd -Test` / `-Benchmark`.

- A solid foreground wall hides a separate rear chunk: two terrain draws and
  24 triangles become one draw and 12 triangles with identical framebuffer
  pixels. Removing culling fails this regression.
- A one-block opening at a negative-coordinate chunk seam reveals the rear
  geometry immediately; partially exposed chunks remain submitted afterward.
- Translation, rotation, running-FOV projection changes, viewport resize,
  framebuffer switching, wireframe transitions, near-plane crossings, and
  flight inside a block match a fresh renderer's pixels on the first frame
  and after queries settle.
- Deliberately unavailable results keep both chunks drawable without reading
  a result. An edit while those results are pending prevents the old hidden
  answer from suppressing newly exposed terrain.
- Existing seam, upload-failure, material, selection, HUD, input, and
  persistence tests continue through the production renderer.
- At seed-0 spawn height, twelve moving sky frames submit no terrain or
  queries. Placing an overhead block produces visible pixels immediately;
  removing it returns to zero draws, and looking down restores terrain.
- CPU tests cover empty meshes, separated surfaces with overlapping combined
  bounds, negative coordinates, all rectangle orientations, near/far planes,
  boundary contact, and rejection beyond a rotated frustum corner.

The benchmark also translates the camera by 0.01 blocks per frame for a fifth
scenario. It checks that moving frames do not reuse hidden-chunk results.
These are hidden graphical checks, not hands-on interactive playtesting.

## Measurements and validation

The [sky-motion follow-up](sky-motion-culling.md) records the current fix and
its measurements. The earlier [five-pair rendering benchmark](occlusion-benchmark.md) supersedes
the single-run timing comparison below. It includes FPS distributions, CPU/GPU
intervals, geometry, uploads, edits, and initialization. It did not establish
an FPS improvement; stationary geometry reductions remain repeatable.

The historical measurements below predate the surface-frustum refinement.
They use seed 0, a 960x540 hidden window, native Windows Release,
Intel UHD Graphics, ten warm-up frames, and 60 measured frames with `glFinish`.
The first four camera positions/pitches are unchanged from the existing
benchmark. The fifth uses the same horizontal view with the small translation
described above. Frame times include the HUD and selection where present.

The baseline is `71c710b`, extracted into a temporary directory outside the
working tree, with only the fifth benchmark scenario and its per-frame draw
accounting added. No user save is opened. Run `.\build.cmd -Benchmark` in each
checkout to reproduce the comparison.
Timing varies with driver scheduling and system load; geometry counts are the
more direct measure of work removed.

One sequential before/after run on September 9, 2026:

| View | Terrain draws before / after | Triangles before / after | Frame ms before / after |
| --- | ---: | ---: | ---: |
| Horizontal, stationary | 42 / 24 | 22,890 / 15,536 | 5.725 / 4.527 |
| Down 30 degrees, stationary | 42 / 23 | 22,890 / 14,234 | 4.956 / 4.869 |
| Sky, stationary | 0 / 0 | 0 / 0 | 5.358 / 4.538 |
| Elevated, down 45 degrees | 48 / 45 | 25,464 / 22,684 | 5.420 / 4.905 |
| Horizontal, moving | 41 / 41 | 22,854 / 22,854 | 4.653 / 4.934 |

Counts are for the last frame; moving candidates can vary during a run. The
moving case issued 41 queries on its last frame and culled nothing. Its measured
frame time increased by 0.281 ms. Stationary cases issued no new queries after
warm-up. All cases had zero mesh rebuilds, buffer uploads, and uniform-location
lookups during measurement. The changed sky timing despite identical geometry
shows why a single run cannot establish a general FPS improvement.

All of these checks exited successfully on September 9, 2026:

```text
Native Windows, Intel UHD Graphics:
  .\build.cmd -Test
  .\build.cmd -Configuration Debug -Test
  .\build.cmd -Benchmark

WSL Ubuntu 24.04, hidden Mesa/Xvfb graphics:
  make -j4
  make test
  make test-sanitize
  make test-gl
  make CC=clang CFLAGS='-O2 -g -Werror' all test
  make CC=clang CFLAGS='-O2 -g -Werror' test-gl
  make test-build
```

At that checkpoint, native Windows incremental-build regressions were not
rerun because build scripts were unchanged. They passed during the later
sky-motion fix. No hands-on interactive playtest was performed.
The next optimization target is safe culling during camera movement, with
the current pixel comparisons retained as a regression oracle.
