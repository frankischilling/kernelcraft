# Player movement increment

Base: `afa6dd0711c712928e4ea8e484f600b200e125ca`, merged `main` after PRs #10
and #11. The checkout was clean, repository access was ADMIN, and issue #9 was
the only open issue. No PRs or applicable AGENTS.md files were present.

The baseline `make -j4 all test test-gl` passed under WSL Ubuntu with GCC 13.3.0,
GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0, and Mesa llvmpipe (LLVM 20.1.2).
Editing, DDA, chunk seams, startup, and hidden-window input/pixel checks work.
Movement is still camera-only flight and the initial position may be in terrain.
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
