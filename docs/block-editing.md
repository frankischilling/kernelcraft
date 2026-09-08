# Block editing checkpoint

This branch starts at `74953fd229483614a44a34925d8f97ea88f4ea48` and depends on
PR #10 (`fix/build-and-input-foundation`). Remote `main` was still `7b078e0`
when work began. Issue #9 tracks the larger sandbox foundation.

The starting world had correct exposed-face meshes and working debug flight.
Meshes were static after startup, and the HUD's raycast sampled every 0.1 unit.
There were no editing controls. The baseline command
`make -j4 all test test-gl` passed in WSL Ubuntu with GCC 13.3.0,
GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0, and Mesa llvmpipe.

## Implemented and tested

1. Separated the CPU world header from renderer declarations. Added validated
   block edits, solidity queries, and dirty propagation to face neighbors.
   Tests cover world limits, no-op edits, material changes, and exact seam neighbors.
2. Rebuild dirty meshes before drawing, reuse existing GPU objects, and expose
   chunk, terrain draw, face/triangle, and rebuild counters. Graphical tests
   verify uploads and pixels after edits, including failed uploads.
3. Replaced sampled selection with bounded DDA traversal. Tests cover entry faces,
   boundary starts, simultaneous crossings, reach, and invalid input.
4. Connected press-only mouse editing and numbered material selection to the
   application, with a crosshair and target outline. Reserve a body-sized space
   around the debug camera to prevent placing blocks through the player.
5. Linux GCC/Clang, CPU sanitizers, native Windows, and application checks pass.
   The independent read-only review's missing seam-identity and upload-failure
   cases were added; its final selection/control review found no further defect.

The coordinate convention remains unchanged: block `(x,y,z)` occupies
`[x,x+1) × [y,y+1) × [z,z+1)` in world units. The finite world spans
`[-128,128)` horizontally and `[0,64)` vertically.

Normal movement with collision, a user seed, save/load, and greedy meshing
remain subsequent increments. Edits in this checkpoint are held in memory.

## Controls and contracts

Run `make run` on Linux or `.\build.cmd -Run` on native Windows. W/A/S/D and
Space/Left Shift retain debug flight; Escape captures/releases the cursor.
Focus loss releases it. Left click breaks a target and right click places on
its face, once per press, within six world units. Keys 1/2/3 select grass, dirt,
and stone. Mouse/key editing is ignored while released, unfocused, or minimized.
The crosshair, gold target outline, and material selector use framebuffer pixels.

The existing initial camera may be inside generated terrain. Hold Space to fly
above it and look down. Movement still passes through blocks. The placement
exclusion uses a 0.6-wide, 1.8-high body, with its eye 1.62 above its feet;
touching a cell boundary is allowed, overlapping a cell is not. This is an edit
restriction, not player collision or a safe-spawn implementation.

`getBlock` returns a borrowed, const block or NULL for invalid/unloaded positions.
`setBlock` rejects unknown IDs and out-of-world coordinates; unchanged values
succeed without dirtying anything. An exposure change dirties the owner and its
face neighbors at X/Z seams. Material-only changes dirty only the owner.
Direct mutable chunk access remains for generation, meshing, and fixtures.
The world owns chunks; mesh builders return staging buffers that their caller
must free. The renderer owns GPU objects and clears a dirty flag only after a
successful upload. A failed update stops drawing and causes application cleanup
with a current context. Empty meshes retain their GPU handles for later reuse.

DDA normalizes direction, clips to the finite world before converting to cell
integers, and includes hits exactly at maximum reach. A boundary start chooses
the cell entered in the travel direction; zero direction components stay in
their half-open cell. Simultaneous crossings advance every tied axis and report
X before Y before Z as the placement face. Edge/corner-only contacts are skipped.
Starting in a solid without crossing a face returns a distance-zero hit with
no placement face: breaking is allowed, placement is rejected. Zero direction,
non-finite input, and negative reach return a miss. The adjacent cell may be
outside the world; the edit operation validates it before use.

## Validation

These commands exited 0 on this increment. Linux commands ran from the repository
under WSL Ubuntu with GCC 13.3.0, Clang 18.1.3, GLFW 3.3.10, GLEW 2.2.0,
freeglut 3.4.0, and Mesa llvmpipe (LLVM 20.1.2). Native Windows used MSYS2
MINGW64 GCC 13.2.0, GLFW 3.3.8, GLEW 2.2.0, freeglut 3.4.0, and Intel UHD
Graphics driver 32.0.101.7077. There was no cross-compilation.

| Command | Result |
| --- | --- |
| `make CC=gcc CFLAGS='-O2 -g -Werror' -j4 all test test-gl` | Pass |
| `make CC=clang CFLAGS='-O2 -g -Werror' -j4 all test test-gl` | Pass |
| `make CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' -j4 all test` | Pass |
| `make CC=clang CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' -j4 all test` | Pass |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=gcc test-sanitize` | Pass; CPU AddressSanitizer/UBSan |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize` | Pass; CPU AddressSanitizer/UBSan |
| `make -B PKG_CONFIG=false test` | Pass; forced CPU rebuild without graphics package discovery |
| `make test-build` | Pass; separate configurations, incremental headers/flags/compiler, dry run |
| `.\build.cmd -Test` | Pass; native Windows Release, including application and renderer |
| `.\build.cmd -Configuration Debug -Test` | Pass; native Windows Debug |

The application harness invokes the registered key/mouse callbacks between real
frames, checks that a block disappears after a click and grass appears after
placement, and reads the crosshair and outline pixels. It performs real hidden
window resizes to 640×360, 360×640, and 1280×720; zero size is injected separately.
Captures of those three frames were visually inspected. The tests also verify
input gating, frame-stall movement limits, projection aspect, dirty flag
consumption, and normal cleanup. To capture them after building `test-gl`:

```sh
KERNELCRAFT_TEST_CAPTURE=/tmp/kernelcraft-edit xvfb-run -a ./bin/linux/Release/test-startup
```

This writes `-0.ppm`, `-1.ppm`, and `-3.ppm` captures at the chosen prefix.
These are automated application checks, not a human playtest or a physical
high-DPI/window-manager test. Collision and restart persistence cannot be tested
yet because they are not implemented. Windows sanitizer runtimes, allocation
failure injection, and macOS were not tested.

The first native renderer run failed with access violation `0xc0000005` in
`igxelpicd64.dll` during a deliberate negative-size index upload in the test.
Stage diagnostics located it after the ordinary edit/pixel checks. The fixture
now uses an invalid usage enum to obtain a real GL error without an allocation
request; both failed vertex and failed index upload cases pass on Intel and
Mesa. This changes the test injection, not the driver. Expected missing-asset
and failed-rebuild diagnostics remain visible.

The original terrain fingerprint still matches: 210,404 exposed faces and
31,981,408 mesh bytes before edits. No new performance comparison is claimed;
the benchmark's four camera views still perform zero buffer uploads and zero
uniform-name lookups during unchanged frames. Timing from concurrent validation
is not comparable to the historical table in `performance.md`.

## Delivery and continuation

Branch: `feat/block-editing`. Draft [PR #11](https://github.com/frankischilling/kernelcraft/pull/11)
depends on unmerged [PR #10](https://github.com/frankischilling/kernelcraft/pull/10).
Core edits/rebuilds are commit `4acb400`; selection/controls are `5791145`.
Neither PR has been merged, and [issue #9](https://github.com/frankischilling/kernelcraft/issues/9)
remains open because its full acceptance criteria are not yet met. Existing
GCC/Clang Actions jobs run the expanded CPU and graphical targets.

Next implement normal player movement: a CPU body/velocity state, bounded fixed
simulation steps, gravity, grounded jumping, axis collision at walls/floors/
ceilings/corners and chunk seams, and safe spawn placement. Keep debug flight
explicit and separate. Reuse the documented body dimensions for placement.
Then add selectable deterministic seeds and a versioned, validated save format
with safe replacement and restart round-trip tests. Greedy meshing follows those
working world operations. The playable-foundation milestone remains incomplete.
