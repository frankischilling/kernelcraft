# Timed hand breaking

Hold left mouse while aiming at a block within six world units. A gold bar above
the crosshair fills until the block disappears. Right mouse places once per press.

| Material | Hand breaking time |
| --- | --- |
| Dirt | 0.5 seconds |
| Grass | 0.75 seconds |
| Stone | 1.5 seconds |
| Cobblestone | 2 seconds |

Walking, crouching, running, and debug flight use these same rates. Every current
hotbar slot uses the hand rate, including empty slots. Suitable tools, drops,
inventory, and crack textures remain planned.

Releasing left mouse discards progress. Losing reach, looking away, aiming at a
different cell, or observing a different material in the cell also restarts from
zero. Moving the aim between faces of the same block keeps progress. If left
mouse stays held, the next valid target starts automatically with zero progress.
Completing a break discards excess time and removes at most one block per frame.

Changing the selected slot, right-clicking, or toggling flight cancels the hold.
Focus loss, released capture, minimization, and zero-sized framebuffers do the
same. Resume with a fresh left press; a held button cannot finish a break after
a pause. Selecting the same slot or receiving a key repeat does not cancel.

## Timing and integration

`src/world/edit.c` owns the CPU-only timer and material durations. Acquiring a
target earns no time. Later updates add active elapsed seconds, capped by
`BREAK_MAX_FRAME_SECONDS` at 0.1 seconds per frame. Negative or non-finite elapsed
time resets progress. Zero time can acquire a target but cannot advance it.
The completion comparison allows one nanosecond for floating-point summation
error. Completion is observed on the first frame that reaches the duration.

`src/utils/inputs.c` starts and cancels the hold through the existing callbacks.
The game loop updates breaking after movement, using the current eye and DDA
selection, before saving and rendering. A completed break calls `setBlock`, so
the existing bounds validation and dirty-neighbor propagation apply. Rendering
rebuilds only dirty chunks; partial progress never rebuilds a terrain mesh.

The HUD draws the progress bar using the existing compatibility renderer and
restores caller OpenGL state. The bar uses the reserved band above the crosshair
and disappears when inactive or captured input is released. It is hidden below
64 pixels wide or 40 pixels high.

Partial progress and held input are session state. Saving records completed
world edits through the existing format; restarting clears any unfinished hold.
This feature changes no block IDs, texture layers, terrain generation, or save
versions. Use `--no-save` or an explicit temporary `--world` path for checks.

## Regression coverage

- `tests/test_edits.c`: material timing at 30/60/120 FPS, just-before and exact
  completion, frame stalls, invalid elapsed time, release reset, changed targets
  and materials, inclusive reach, negative-coordinate seam invalidation, and no
  progress carried into the next block.
- `tests/app_smoke.c`: real callbacks, all pause paths, slot/right-click/flight
  cancellation, empty-slot breaking, held progress across the actual frame loop,
  HUD pixels, completed edits, and clean neighbor meshes after rendering.
- `tests/test_hud.c`: partial bar pixels, visibility while captured, responsive
  landscape/portrait layouts, crosshair and hotbar preservation, and GL state.
- `tests/app_persistence.c`: a timed callback edit followed by F5, normal-exit
  saving, and a second process restoring the world with no pending hold.

These harnesses use real world, rendering, and persistence code with controlled
GLFW input delivery. They do not establish physical mouse behavior or how the
chosen hand rates feel during interactive play.

## Validation record

The following commands passed for this change:

| Environment | Command | Result |
| --- | --- | --- |
| Native Windows, Intel UHD Graphics | `.\build.cmd -Test` | Exit 0; CPU, hidden graphics, application, restart, and rendering checks |
| Native Windows, Intel UHD Graphics | `.\build.cmd -Configuration Debug -Test` | Exit 0; same suites in Debug |
| Ubuntu under WSL | `make -j4` and `make test` | Exit 0; GCC build and CPU checks |
| Ubuntu under WSL | `make test-sanitize` | Exit 0; CPU checks with AddressSanitizer and UBSan |
| Ubuntu under WSL, Mesa llvmpipe/Xvfb | `make test-gl` | Exit 0; hidden HUD, shader, application, restart, and rendering checks |
| Ubuntu under WSL | `make CC=clang CFLAGS='-O2 -g -Werror' all test` | Exit 0; Clang build and CPU checks |
| Ubuntu under WSL | `make test-build` | Exit 0; incremental builds, configuration changes, and dependency tracking |

The initial application regression failed on main's immediate-removal behavior
before the timer was implemented. The expanded harness passes with held input.
No hands-on interactive playtest was performed. Native Windows sanitizer checks
were not run; the existing sanitizer target runs on Linux. Build scripts and
dependency discovery are unchanged.
