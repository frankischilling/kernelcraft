# Foundation checkpoint

Audit base: `7b078e0a54d428bc97553a9bd998e8fc0a3953e2` (September 8, 2026).
`origin/main` matched this commit after fetching. The checkout was clean, there
were no applicable AGENTS.md or contribution instructions, no open issues or
PRs, and no Actions workflows. PR #7 had already delivered chunk meshes and
native Windows builds. Work continues on `fix/build-and-input-foundation`.

## Observed baseline

- Native Windows: `.\build.cmd -Test` passed with MSYS2 MINGW64 GCC 13.2.0 and
  Intel UHD Graphics. This includes a hidden application launch, shader/texture
  fixtures, CPU tests, render checks, and normal shutdown.
- Linux under Ubuntu WSL: `make -j4 && make test` passed with GCC 13.3.0,
  GLFW 3.3.10, GLEW 2.2.0, and freeglut 3.4.0. Clang 18.1.3 is available.
- `timeout 5s xvfb-run -a make run` reached the application loop without a
  startup diagnostic; the timeout ended it. This does not verify normal exit.
- From `/tmp`, `xvfb-run -a /mnt/c/Users/imike/kernelcraft/bin/minecraft_clone`
  failed with `Failed to open shader file: assets/shaders/vertex_shader.glsl`.
- `make -n CONFIGURATION=Debug` only copied assets; it did not build debug
  objects. `gcc -std=c11 -Wall -Isrc -c src/math/math.c` failed because `M_PI`
  is not part of C11. Build flags and compiler changes were not dependencies.

The renderer already caches indexed exposed-face meshes, batches four separate
textures, caches uniform locations, and culls occupied chunk bounds using the
rendering projection. Grass top, side, and bottom material and triangle winding
have CPU tests. There is no greedy mesher, integrated atlas, shadow map, or true
occlusion culling. `atlast.py` has a stale input path and no stable tile ordering;
its generated image is unused. Existing textures and attribution are retained.

Framebuffer dimensions already control projection, viewport, and text coordinates;
zero framebuffers skip drawing. GPU cleanup already precedes context destruction.
Chunk allocation uses zero initialization and cleans up partial failure. Mesh
staging buffers belong to callers and are freed after GPU upload. CPU tests need
graphics headers through the existing interfaces but create no window or context.

Blocks occupy `[x,x+1)`, `[y,y+1)`, `[z,z+1)`; their centers are offset by 0.5.
The finite world spans X/Z `[-128,128)` and Y `[0,64)`. Queries reject out-of-range
coordinates. Some conversion helpers still assume unit blocks and fixed dimensions.
The fixed Perlin permutation produces repeatable terrain without wall-clock state,
but there is no selectable seed or demonstrated cross-platform bitwise guarantee.

The HUD calls the existing fixed-step raycast. It lacks a face/placement result
and can skip thin intersections. Movement is free flight. Mouse look continues
with the cursor released, and focus/capture changes retain stale coordinates.
There is no validated edit API, dirty mesh propagation, collision, gravity,
jumping, hotbar, or persistence.

## Implementation order

Tracked prerequisite: [issue #8](https://github.com/frankischilling/kernelcraft/issues/8).

- [x] Repair Linux configuration, dependency discovery, build-option/header
  tracking, and executable-relative assets. Retain the Windows toolchain and
  verify both builds. Extend the real application smoke test to Linux.
- [ ] Repair cursor/focus transitions and test them through the application
  callbacks. Exercise framebuffer changes, zero dimensions, and context lifetime.
- [ ] Add GCC/Clang CI, reconcile build/controls/roadmap documentation, and
  publish the tested checkpoint as a draft PR.

Next, [issue #9](https://github.com/frankischilling/kernelcraft/issues/9) orders
validated edits and dirty mesh uploads, DDA selection and editing controls,
fixed-step player collision, and seeded generation with versioned saves. Greedy
meshing follows correct visible faces and editable chunk seams. The existing
Cube World direction, Fire Bugs, Goblins, and Cupid Sponge remain planned.

The playable-foundation milestone remains incomplete until navigation, editing,
collision, affected-chunk updates, and persistence across restart are exercised
in the running game. Hidden graphical tests are not physical-GPU playtesting.

## Build increment validation

After the first repair, the following commands exited 0:

- Linux/WSL: `make -j4`, `make test`, `make test-build`, `make -j4 test-gl`.
- Native Windows: `.\build.cmd -Test`.

Before the repair, `sh tests/test_build.sh` failed because Release had no
configuration-specific object directory. The application smoke script failed
from its temporary working directory because shaders could not be found.
Both regressions now pass. Missing-shader and missing-texture fixtures fail as
expected. Shader tests use each configuration's output directory to avoid
sharing temporary fixtures across platforms.

The existing benchmark's pixel and submission checks passed on Intel UHD Graphics
and Mesa llvmpipe. Timings collected during build validation are not a controlled
performance comparison; no performance improvement is claimed here.
