# Crouch and running

This increment starts at `dc4f2c54086d0f8d02f3c4efd47d4cd3a3a963d8`, current
`main` after hotbar PR #33. No open PRs remained when work started. The existing
native Windows Release build and its CPU, graphics, input, and persistence
fixtures passed; WSL Ubuntu also passed `make -j4` and `make test` before changes.
The movement work is on `feat/crouch-running` for
[issue #34](https://github.com/frankischilling/kernelcraft/issues/34).
This document describes implementation on the feature branch, not merged delivery.

## Controls and collision

| Mode | Body height | Eye above feet | Horizontal speed |
| --- | --- | --- | --- |
| Walking | 1.8 | 1.62 | 4.5 units/second |
| Crouching | 1.0 | 0.9 | 1.5 units/second |
| Running | 1.8 | 1.62 | 7.0 units/second |

The body stays 0.6 units wide. Hold either Shift key in walking mode to crouch.
The one-block height permits travel through one-block-high passages in the
current full-block world. Body and eye height change on the next simulation
tick, with no interpolation. Releasing Shift retries standing at each tick;
the entire standing body must fit inside world bounds and outside solid blocks.
Feet do not move during the posture change. A blocked stand retains crouch
speed and cannot run. There is no protection against walking off a ledge.

Two W press edges, separated by a release and at most `PLAYER_RUN_TAP_SECONDS`
(0.25 seconds), start running. The endpoint counts; key repeats and duplicate
presses without a release do not. The first W release preserves its tap for the
second press. Releasing W after a run clears it; the next press starts a new
sequence. S, either Shift, entering flight, and pauses clear run/tap state.
Crouching or holding S cannot arm a run. A fresh pair of taps is required after
cancellation. W plus A/D is normalized before applying the speed, and camera
pitch does not affect walking direction or speed.

All postures use the existing X/Z/Y body sweep, world bounds, 120 Hz simulation,
eight-step frame limit, gravity, and one grounded jump per press. Crouching and
running allow jumping with the existing impulse; a low ceiling clips the jump.
Running continues in the air while W stays held. Returning from flight resets
posture to a validated standing position. Flight preserves its camera position
when entered and still uses Space/Left Shift to rise/descend through terrain.

Capture release, focus loss, iconification, and zero framebuffers discard run
history, queued jumps, and simulation backlog. Pausing retains the current body
and falling velocity; active simulation checks Shift and standing clearance
again after resume. Holding Shift across resume crouches again, but holding W
cannot restart a canceled run without another double-tap.

`src/world/player.c` owns posture, body queries, speed, and the CPU-only tap
state. `src/utils/inputs.c` supplies registered W events and polled movement,
shares the active posture with editing, and handles cancellation and snapshots.
The main loop passes crouch/run state to the HUD; the compact status and F3
diagnostics show it. Control hints use the existing available-space checks.

## Saves and placement

Placement rejects overlap with the active walking body, so placing a block
above a crouched head can create a ceiling that prevents standing. Flight keeps
standing-sized placement exclusion. DDA reach, material mapping, dirty-neighbor
updates, meshes, textures, terrain generation, and save serialization are unchanged.

Save versions 1 and 2 remain readable, and writes still use version 2. Restart
always stands with zero velocity and no pending tap sequence. A crouched save
uses the current feet if the standing body fits. Otherwise it uses the existing
safe-spawn search above the nearest available column, as flight saves already
do. This can restore the player above the ceiling. Snapshotting does not move
the live player; if no safe standing position exists, saving reports failure and
preserves the previous file. Persisting crouched posture would require a future
format change with a separate compatibility policy.

## Validation

These commands exited 0:

| Environment | Command |
| --- | --- |
| WSL Ubuntu, GCC 13.3.0 | `make -j4` |
| WSL Ubuntu, GCC 13.3.0 | `make test` |
| WSL Ubuntu, GCC 13.3.0 | `make test-sanitize` |
| WSL Ubuntu, GCC 13.3.0, Mesa/Xvfb | `make test-gl` |
| WSL Ubuntu | `make test-build` |
| WSL Ubuntu, Clang 18.1.3 | `make CC=clang CFLAGS='-O2 -g -Werror' all test` |
| WSL Ubuntu, Clang 18.1.3, Mesa/Xvfb | `make CC=clang CFLAGS='-O2 -g -Werror' test-gl` |
| Native Windows, MINGW64 GCC 13.2.0 | `.\build.cmd -Test` |
| Native Windows, MINGW64 GCC 13.2.0 | `.\build.cmd -Configuration Debug -Test` |

The first new application regression failed on the unchanged standing eye
height after Shift input. It passed after the controls reached the player model.
CPU tests cover the exact double-tap endpoint and adjacent doubles, repeats,
clock reversal/invalid values, posture timing, ceiling and world-top clearance,
speed across frame partitions, diagonal movement, jumping, and running into
walls across negative chunk seams. Existing walking/collision tests remain.

The application harness exercises registered callbacks and the real renderer.
It covers both Shift keys, crouched placement, blocked standing after all pause
paths, canceled run/tap sequences, missing key-event polling, flight transitions,
and rendered crouch/run frames. It now completes 55 loop iterations with 53
rendered frames and two paused waits. Separate process fixtures exercise F5,
normal-exit saves, restart above a low ceiling, unchanged world bytes on re-save,
and the existing error paths. Every fixture uses temporary paths or `--no-save`.

Linux execution uses WSL Ubuntu and Mesa/Xvfb. Native Windows uses the installed
MSYS2 MINGW64 toolchain and Intel UHD Graphics. These are automated checks,
including substituted input delivery, rather than hands-on interactive playtests.
The captured crouching and running frames were visually inspected for HUD
labels, controls, hotbar, and camera height. Physical focus/minimize behavior,
control feel, and high-DPI monitor behavior remain unverified.

The next gameplay increment can address timed block breaking from the README.
It needs an explicit hand/tool timing policy and cancellation rules for changed
targets and pauses. Inventory, creatures, and the longer-term Cube World
direction remain outside this movement increment.
