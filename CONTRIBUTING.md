# Contributing

Keep changes focused on the native C/OpenGL game. Check existing issues before
opening a duplicate, branch from current `main`, and link the issue in the PR.
Describe the behavior, validation commands, and remaining uncertainty. Preserve
license notices and contributor attribution.

Use C11 and the repository's `.clang-format`; keep GLEW before headers that
include OpenGL. Format changed code without reformatting unrelated files.
Treat CPU world/mesh ownership separately from GPU objects. Destroy GPU objects
while their context is current. Text currently needs OpenGL 3.3 compatibility,
so requesting a core profile requires replacing that renderer first.

Run `make test` for CPU changes. Use `make test-sanitize` on Linux, `make test-gl`
for rendering/startup changes, and `make test-build` for Makefile changes.
`make CC=clang CFLAGS='-O2 -g -Werror' all test` provides another compiler check.
On native Windows, use `.\build.cmd -Test` and repeat with
`-Configuration Debug` when changing startup or build behavior. See the README
for dependencies and `docs/windows-incremental-build.md` for the current continuation point.
For Windows build changes, also run `powershell -NoProfile -ExecutionPolicy Bypass -File tests\test_build.ps1`
or native `make test-build`. This checks the object cache in a temporary project copy.
Run Debug and sanitizer builds sequentially in one checkout: `test-sanitize`
also writes the Debug output directories.

Terrain material IDs map to base texture-array layers 0..3. The fragment shader
selects per-block variants from layers 4..6 using world position and seed.
Keep that ordering consistent across the mesher, renderer, and shader. New layers must match the existing
tile dimensions; preserve repeated UVs and extend the independent material
pixel comparisons when changing face mapping or sampling.

Add tests for observable defects and boundary cases. CPU tests must not create
an OpenGL context. The application smoke harness substitutes GLFW event/input
delivery and zero framebuffer size; it performs real landscape/portrait resizes while running the actual game loop and OpenGL
renderer. It cannot establish physical monitor scaling, window-manager behavior,
or whether controls feel right during interactive play.

For manual input checks: launch the game, navigate with W/A/S/D, release the
cursor with Escape, and confirm mouse/keyboard movement stops. Capture again and
check that the view does not jump, then switch applications and return. Focus loss releases the
cursor; Escape resumes capture. Resize, minimize, and restore the window. Test
minimize during a fall and check the first mouse movement after restore for a
view jump. The automated fixture covers iconification with positive framebuffer
dimensions independently of focus loss, plus zero-size pauses. Toggle F3 while captured and released;
check that labels and material slots fit small landscape/portrait windows, save
errors stay readable, and the aiming area remains clear.
Select all nine hotbar slots with 1–9. Grass, dirt, and stone occupy slots 1–3;
empty slots 4–9 should break blocks without placing anything. Check the flat
icons and selected border in small landscape, portrait, and wide/short windows.
Break and place blocks at chunk seams, and check
that the outline follows the next target. Try placement near the camera and
while the mouse is released. Use a new explicit `--world` path for manual tests.
Save with F5, close, reopen without `--seed`, and verify edits, feet, view, and
selected slot, including an empty slot. Report the
platform, driver, and what you observed. Walk into walls and corners, jump under
a low ceiling, cross negative-coordinate chunk seams, and break the supporting
block. Check that holding Space does not repeat jumps and that walking speed
stays constant when looking up. Use F to test flight and returning from inside
terrain. Hold either Shift key, enter a one-block-high passage, and release Shift;
the player must stay crouched until the full standing body clears the ceiling.
Try placing a ceiling above the crouched head and a block inside the body.
Double-tap W to run, add A/D, and run into walls and corners. Check that W release,
S, Shift, flight, capture release, focus loss, minimization, and a zero framebuffer
cancel running and require a fresh double-tap. Save while crouched beneath a
ceiling, then restart and check the safe standing position and preserved edits.
Check pause/resume during a fall. Use `--no-save` for disposable sessions. Automated persistence fixtures use
unique temporary directories and preserve any existing user saves. CPU tests
cover malformed files and allocation/write/flush/sync/close/replace failures;
two-process graphical fixtures cover actual edit callbacks, F5, normal-exit
saves, and restored rendered chunks. Issue #9 tracks merged foundation delivery.
