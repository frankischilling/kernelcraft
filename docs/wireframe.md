# Terrain wireframe

Press F4 to toggle terrain between solid faces and mesh edges. The HUD shows
whether wireframe is on when its status line fits the framebuffer. Small windows
shorten this line using the existing HUD layout rules.

Wireframe draws the triangles already submitted by the chunk renderer. Greedy
rectangles have diagonal edges, and chunk boundaries can split a flat surface.
The edges retain the existing terrain textures and lighting. Faces are unfilled,
so edges behind them can be visible. The mode does not add occlusion culling or
expose enclosed block faces that the mesher has removed.

Selection still uses the nearest solid voxel within six units. Its gold outline
and face tint remain enabled, as do the filled HUD, hotbar icons, crosshair, and
breaking bar. Collision, placement, hand-breaking times, and saves behave as in
solid mode. Toggling during a held break keeps its progress.

F4 responds once per press. Repeats and releases do not toggle it. It works with
the cursor captured or released, without changing capture or resuming movement.
Unfocused, minimized, and zero-framebuffer windows ignore it. Pausing or changing
between walking and debug flight retains the chosen mode.

Every new or loaded session starts with solid terrain. Wireframe is not saved.
Existing save versions, block IDs, seeded worlds, texture layers, and shaders
are unchanged.

## Implementation

`src/utils/inputs.c` keeps the session flag in `InputState`. `src/main.c` passes
it explicitly to `renderWorld` and the HUD. The renderer selects `GL_LINE` or
`GL_FILL` only around terrain draws, then restores the caller's front and back
polygon modes separately. The existing OpenGL 3.3 compatibility requirement
remains in place.

Both views use the same chunk meshes, material array, frustum checks, render
radius, and one terrain draw per visible chunk. Switching views does not dirty
chunks or upload buffers. Edits still rebuild their affected chunks before
drawing. This is a diagnostic view, with no frame-time improvement claimed.

## Regression coverage

- `tests/app_smoke.c` exercises registered input handling, pause restrictions,
  actual F4 frame-loop output, filled crosshair and breaking-bar pixels, and
  timed breaking while wireframe is active.
- `tests/render_benchmark.c` probes a known prism's interior: filled faces cover
  the sample, while wireframe leaves only triangle diagonals. It compares the
  pixels before and after toggling back, checks unchanged geometry/draw counts
  and no uploads, preserves different front/back polygon modes, and edits a
  negative-coordinate chunk seam in wireframe.
- `tests/terrain_render_checks.h` checks the same distance and frustum culling
  in wireframe, alongside the independent material and repeat-UV comparisons.
- `tests/test_hud.c` checks the displayed state, icon pixels, breaking bar,
  responsive layout, and restoration of caller GL state.
- `tests/app_persistence.c` saves with wireframe enabled and checks that a new
  process starts in solid mode with the edited world and selected slot intact.

These are CPU and hidden graphical harnesses using the real renderer, game
loop, and save code with controlled GLFW input delivery. They do not establish
physical keyboard behavior, window-manager transitions, or interactive feel.

## Validation record

The change starts from `7d3e8dd41ec000030dfe9aa2dac54a35449e09c4`, after the
timed-breaking merge. There were no open pull requests when work began.

| Environment | Command | Result |
| --- | --- | --- |
| Ubuntu under WSL, GCC | `make -j4` and `make test` | Exit 0; application build and CPU suites |
| Ubuntu under WSL, GCC | `make test-sanitize` | Exit 0; CPU AddressSanitizer and UBSan checks |
| Ubuntu under WSL, Mesa llvmpipe/Xvfb | `make test-gl` | Exit 0; rendering, HUD, actual application loop, and process restart |
| Ubuntu under WSL | `make test-build` | Exit 0; incremental build regressions |
| Ubuntu under WSL, Clang | `make CC=clang CFLAGS='-O2 -g -Werror' all test` | Exit 0; application build and CPU suites |
| Ubuntu under WSL, Clang/Mesa | `make CC=clang CFLAGS='-O2 -g -Werror' test-gl` | Exit 0; final graphical and restart suites |
| Native Windows, Intel UHD Graphics | `.\build.cmd -Test` | Exit 0; final Release CPU and graphical suites |
| Native Windows, Intel UHD Graphics | `.\build.cmd -Configuration Debug -Test` | Exit 0; final Debug CPU and graphical suites |

The first application regression failed before implementation: F4 left all
9,216 wall-interior pixels filled. Both drivers now produce 96 lit edge pixels
in wireframe and 9,216 when toggled back to solid. Native captures of the wall
and a held break were inspected. Formatting and whitespace checks passed, and
read-only code review found no remaining issues.

No hands-on interactive playtest or native Windows sanitizer run was performed.
Build scripts and dependency discovery are unchanged. Temporary test worlds and
`--no-save` sessions preserved the existing user save and checkout changes.
