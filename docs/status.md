# Foundation checkpoint

This records the historical build/input increment in PR #10, now merged.
PR #11's block editing increment is also merged. The current continuation is
[player movement](player-movement.md), on `feat/player-movement` in PR #12.
The observations below describe the earlier checkpoint.

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
- [x] Repair cursor/focus transitions and test them through the application
  callbacks. Exercise framebuffer changes, zero dimensions, and context lifetime.
- [x] Add GCC/Clang CI, reconcile build/controls/roadmap documentation, and
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

## Input and framebuffer increment

The shared `tests/app_smoke.c` runs the real application entry point and renderer.
It supplies cursor/key/focus events to the registered callbacks and substitutes
framebuffer dimensions: landscape, portrait, zero size, and twice the original
pixel dimensions. Assertions check camera changes, one-unit free-flight movement,
paused movement while released/unfocused/minimized, Escape repeat handling,
recapture without jumps, viewport, the uploaded projection aspect ratio, and
normal shutdown with a current context and no GL errors.

The new input assertion failed against the prior code because released-cursor
motion changed yaw. With the fix, the first mouse sample after capture/focus
changes resets the delta. Focus loss releases the cursor, and returning requires
Escape to capture again. Minimized frames now skip movement as well as rendering.
Debug-flight displacement is bounded to 0.1 seconds per frame; this is not normal
player physics or a fixed simulation timestep.

The application explicitly requests OpenGL 3.3 compatibility for GLSL 330 and
FreeGLUT bitmap text. See the [GLFW context hints](https://www.glfw.org/docs/3.3/window_guide.html#window_hints_ctx)
and [cursor modes](https://www.glfw.org/docs/3.3/input_guide.html#cursor_mode).
Physical high-DPI behavior and interactive window-manager focus transitions
remain unverified; the automated tests inject those inputs and dimensions.

Review found that the build-settings recipe executed during `make -n` on a fresh
checkout. A new regression reproduced it; moving the write into the shell recipe
made dry runs leave the output tree absent. The compiler override test now keeps
CFLAGS constant so the compiler change alone must trigger rebuilding.

## Validation and handoff

Native Windows used MSYS2 MINGW64 GCC 13.2.0, GLFW 3.3.8, GLEW 2.2.0,
freeglut 3.4.0, and Intel UHD Graphics driver 32.0.101.7077. Linux tests ran on
Ubuntu under WSL with GCC 13.3.0/Clang 18.1.3 and Mesa llvmpipe (LLVM 20.1.2).
These are native Windows and Linux/WSL executions, not cross-compilation.

| Command | Observed result |
| --- | --- |
| `make -j4 all test-gl` | Pass, exit 0; real hidden application, shaders, textures, terrain pixels, input, viewport/projection, normal shutdown |
| `make CC=gcc CFLAGS='-O2 -g -Werror' -j4 all test test-gl` | Pass, exit 0 |
| `make CC=clang CFLAGS='-O2 -g -Werror' -j4 all test test-gl` | Pass, exit 0 |
| `make CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' -j4 all test` | Pass, exit 0 |
| `make test-sanitize` | Pass with GCC, exit 0; AddressSanitizer and UBSan CPU checks |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize` | Pass, exit 0 |
| `make test-build` and `CC=clang make test-build` | Pass, exit 0; controlled GCC-to-Clang switch, header/flags, separate outputs, dry runs |
| `.\build.cmd -Test` | Pass, exit 0; native Windows Release |
| `.\build.cmd -Configuration Debug -Test` | Pass, exit 0; native Windows Debug |
| `make GRAPHICS_LDLIBS= check-deps` | Expected failure, exit 2 with dependency instructions |
| `make CONFIGURATION='Release unexpected' -n all` | Expected failure, exit 2 with accepted configuration names |
| `MESA_GL_VERSION_OVERRIDE=3.0 xvfb-run -a ./bin/linux/Release/test-startup` | Expected failure, exit 1 with the required context version |

`tests/test_startup.sh` also checks missing shader/texture exit code 1 and rejects
harness failures. Compilation and diagnostics are shown directly; no shared build
log is overwritten. The GCC/Clang Actions jobs repeat Release/Debug builds,
CPU sanitizers, and Mesa graphical tests on Ubuntu 24.04 with read-only repository
permissions. See [PR #10](https://github.com/frankischilling/kernelcraft/pull/10)
for hosted run results. Windows validation is local; there is no Windows CI job.

The independent read-only review identified dry-run and compiler-test problems,
and a missing positive movement frame in the application test. Each was addressed.
The movement frame now verifies the game-loop input connection and stall clamp.
No human reviewer approval has been requested or claimed.

The prerequisite repair and subsequent block editing increment were merged in
PRs #10 and #11. See the player movement checkpoint for current results.
Selectable seeds, save/load, greedy meshing, and the later roadmap remain undone.
Interactive navigation/editing/collision/persistence, physical high-DPI changes,
allocation-failure injection, macOS, and Windows sanitizer runs were not performed.
