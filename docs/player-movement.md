# Player movement increment

Base: `afa6dd0711c712928e4ea8e484f600b200e125ca`, merged `main` after PRs #10
and #11. The checkout was clean, repository access was ADMIN, and issue #9 was
the only open issue. No open PRs or applicable AGENTS.md files were present.

The baseline `make -j4 all test test-gl` passed under WSL Ubuntu with GCC 13.3.0,
GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0, and Mesa llvmpipe (LLVM 20.1.2).
Editing, DDA, chunk seams, startup, and hidden-window input/pixel checks work.
At that base, movement was camera-only flight and the initial position could be
in terrain.
Seeds and persistence are not implemented.

## Design and implementation sequence

1. Add a CPU player state with feet position, velocity, grounded state, and a
   fixed-step accumulator. Share the existing 0.6-wide, 1.8-high body and
   1.62-unit eye offset with placement exclusion. Use 120 steps per second,
   at most eight per frame; discard excess elapsed time after a stall.
2. Sweep the body along X, Z, then Y against solid cells and finite world bounds.
   Clip movement to the first blocking face, allowing tangential movement.
   Walking speed is 4.5 units/second; gravity is 24 units/second squared,
   jump speed 8 units/second, terminal fall speed 50 units/second. Normalize
   diagonal input and keep walking direction independent of camera pitch.
3. Find a clear standing position above terrain near the preferred column.
   Normal movement becomes the default; F toggles debug flight. Returning from
   flight validates the body or searches for a safe nearby spawn. No automatic
   step climbing, crouch, interpolation, fall damage, or moving-platform system.
4. Test floors, walls, ceilings, corners, seams, finite bounds, jump presses,
   removed support, timing partitions and stalls. Connect the model to the
   registered application callbacks and verify movement, collision, mode changes,
   paused input, camera synchronization, and existing edits in rendered frames.
5. Run GCC/Clang, native Windows, CPU sanitizers, build regressions, and graphical
   checks. Review and publish a draft PR with a concrete continuation record.

The finite world is closed to normal movement at its bottom, top, and horizontal
edges. Debug flight remains unrestricted. Blocks retain their half-open cell
convention and current textures. The next dependency is selectable deterministic
seeds and validated save/load with restart tests; issue #9 stays open until the
full foundation acceptance criteria are met.

## Implemented behavior

The CPU model and tests are commit `68f8aec`; the application integration and
controls are `86fe5a4`. The game starts walking on a clear surface. W/A/S/D uses
horizontal camera yaw and normalized diagonal input. Space requests one grounded
jump per press; repeat events and holding Space do not repeat jumps. F enables
camera-only flight; Space/Left Shift rise/descend in that mode. F returns to the
current clear body position or searches above terrain near its column. If no
standing column is available, flight continues with a HUD message.

The player body occupies a continuous axis-aligned box. Face contact is allowed;
overlap is rejected. Each fixed step sweeps X, then Z, then Y through solid cells,
clipping at the nearest face and zeroing blocked velocity. This allows wall
sliding without automatic stepping. Support is recomputed before jumping, so
removing the floor does not allow a stale grounded jump. Placement uses the same
body bounds and the exact normal-player feet position.

The main loop owns the input/player state and synchronizes the camera eye after
simulation. Escape, focus loss, and a zero-sized framebuffer pause physics and
discard queued jumps and fractional elapsed time. Falling velocity is retained
while paused. The HUD reports movement state and completed steps per frame;
this counter is not a measured ticks-per-second rate.

## Validation

All commands below exited 0. Linux execution was under WSL Ubuntu with GCC
13.3.0, Clang 18.1.3, GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0, and Mesa
llvmpipe (LLVM 20.1.2). Native Windows used MSYS2 MINGW64 GCC 13.2.0,
GLFW 3.3.8, GLEW 2.2.0, freeglut 3.4.0, and Intel UHD Graphics driver
32.0.101.7077. These were actual native/WSL executions, not cross-compilation.

| Command | Result |
| --- | --- |
| `make -j4 all test test-gl` | Baseline passed before changes |
| `make CFLAGS='-O2 -g -Werror' -j4 all test test-gl` | GCC Release passed during integration |
| `make CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' -j4 all test test-gl` | Final code passed, including flight-return regression |
| `make CC=clang CFLAGS='-O2 -g -Werror' -j4 all test test-gl` | Final code passed |
| `make CC=clang test-sanitize` | CPU AddressSanitizer/UBSan passed |
| `make test-build` | Incremental headers, flags, compiler changes, configurations, and dry-run checks passed |
| `.\build.cmd -Configuration Release -Test` | Final native Windows code passed |
| `.\build.cmd -Configuration Debug -Test` | Final native Windows code passed |

`tests/test_player.c` covers generated/blocked spawn, rejected positions, floors,
ceilings, world limits, terminal-speed landing, removed support, grounded and
airborne jump requests, walls on both sides of positive/negative chunk seams,
corners and sliding, diagonal speed, frame-time partitions, and stalled frames.
The first test compile failed because the player module did not yet exist.

The real application harness runs 50 loop iterations with 48 rendered frames
and two minimized waits. Registered callbacks control walking into a wall,
jumping once while Space remains held, refusing placement inside the body,
breaking the support block, falling, pausing, and resuming. It also exercises
safe returns from flight inside/outside the world, pitch-independent walking,
camera synchronization, and the existing selection/edit pixel and resize checks.
The startup script retains missing-asset failure and cleanup coverage.

Independent read-only review found two numerical defects. First, an endpoint
just before a wall could round into it even when the sweep did not clip. Second,
subtracting the eye offset could put stationary feet just below a floor when
toggling flight, causing an unnecessary respawn. Both failed new regression
checks before their fixes. Collision now checks rounded contact positions;
flight derives feet from displacement relative to the original exact standing
height. The final focused review reported no remaining findings. No human
reviewer approval is claimed.

To capture flight, portrait editing, resized editing, grounded walking, and a
jump after building the graphical tests:

```sh
KERNELCRAFT_TEST_CAPTURE=/tmp/kernelcraft-player xvfb-run -a ./bin/linux/Release/test-startup
```

This produces PPM frames `-0`, `-1`, `-3`, `-18`, and `-20`. Flight, grounded,
and airborne captures were visually inspected in this increment. The harness
supplies input and time; it is not a human playtest or a physical high-DPI test.

Terrain generation and texture assets are unchanged. The original world still
has 210,404 exposed faces and 31,981,408 mesh bytes before edits. Idle benchmark
views retain zero uploads and zero uniform-name lookups. Concurrent validation
timings are not a controlled performance comparison; no speedup is claimed.

## Delivery and continuation

Branch `feat/player-movement` is published as draft
[PR #12](https://github.com/frankischilling/kernelcraft/pull/12) against `main`.
The existing GCC/Clang Actions jobs run the expanded CPU and application tests;
GCC and Clang jobs passed for integration commit `86fe5a4`. See the PR for the
latest hosted results. This increment has not been merged.
[Issue #9](https://github.com/frankischilling/kernelcraft/issues/9) remains open.

Build/run with `make run` on Linux or `.\build.cmd -Run` on Windows. See the
README for dependencies and controls, and CONTRIBUTING for manual collision
checks. Interactive feel, physical display scaling/focus transitions, macOS,
Windows sanitizers, and allocation-failure injection remain unverified.
There is no automatic stepping, crouch, interpolation, fall damage, or moving
platform behavior. Closed world bounds prevent walking out of the finite map;
debug flight can cross them.

Next implement selectable deterministic seeds and a versioned save format.
Choose full chunks or base-plus-edits explicitly; preserve seed/generator version,
block edits, and relevant player state. Validate bounded headers/counts/IDs and
reject truncated or unsupported data before changing the live world. Use safe
file replacement and verify edited blocks after restarting the actual game.
Then proceed to greedy meshing with material/UV compatibility tests. The current
fixed terrain is repeatable, but seed selection and persistence are absent:
**all edits disappear on exit**. The complete sandbox foundation is not finished.
