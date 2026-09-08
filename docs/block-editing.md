# Block editing checkpoint

This branch starts at `74953fd229483614a44a34925d8f97ea88f4ea48` and depends on
PR #10 (`fix/build-and-input-foundation`). Remote `main` was still `7b078e0`
when work began. Issue #9 tracks the larger sandbox foundation.

The starting world had correct exposed-face meshes and working debug flight.
Meshes were static after startup, and the HUD's raycast sampled every 0.1 unit.
There were no editing controls. The baseline command
`make -j4 all test test-gl` passed in WSL Ubuntu with GCC 13.3.0,
GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0, and Mesa llvmpipe.

## Implementation sequence

1. Separate the CPU world header from renderer declarations. Add validated
   block edits, solidity queries, and dirty propagation to face neighbors.
   Cover world limits, no-op edits, material changes, and chunk seams.
2. Rebuild dirty meshes before drawing, reuse existing GPU objects, and expose
   accurate chunk, draw, face, and rebuild counters. Verify uploads and pixels
   after edits in the graphical regression test.
3. Replace sampled selection with bounded DDA traversal. Test entry faces,
   boundary starts, simultaneous crossings, reach, and invalid input.
4. Connect press-only mouse editing and numbered material selection to the
   application, with a crosshair and target outline. Reserve a body-sized space
   around the debug camera to prevent placing blocks through the player.
5. Run Linux GCC/Clang, sanitizers, native Windows, and application checks;
   review the diff, publish the stacked draft PR, and record remaining work.

The coordinate convention remains unchanged: block `(x,y,z)` occupies
`[x,x+1) × [y,y+1) × [z,z+1)` in world units. The finite world spans
`[-128,128)` horizontally and `[0,64)` vertically.

Normal movement with collision, a user seed, save/load, and greedy meshing
remain subsequent increments. Edits in this checkpoint are held in memory.
