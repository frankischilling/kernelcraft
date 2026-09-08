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
for dependencies and `docs/world-persistence.md` for the current continuation point.

Add tests for observable defects and boundary cases. CPU tests must not create
an OpenGL context. The application smoke harness substitutes GLFW event/input
delivery and zero framebuffer size; it performs real landscape/portrait resizes while running the actual game loop and OpenGL
renderer. It cannot establish physical monitor scaling, window-manager behavior,
or whether controls feel right during interactive play.

For manual input checks: launch the game, navigate with W/A/S/D, release the
cursor with Escape, and confirm mouse/keyboard movement stops. Capture again and
check that the view does not jump, then switch applications and return. Focus loss releases the
cursor; Escape resumes capture. Resize, minimize, and restore the window.
Select each material with 1/2/3, break and place blocks at chunk seams, and check
that the outline follows the next target. Try placement near the camera and
while the mouse is released. Use a new explicit `--world` path for manual tests.
Save with F5, close, reopen without `--seed`, and verify edits, feet, view, and
selected material. Report the
platform, driver, and what you observed. Walk into walls and corners, jump under
a low ceiling, cross negative-coordinate chunk seams, and break the supporting
block. Check that holding Space does not repeat jumps and that walking speed
stays constant when looking up. Use F to test flight and returning from inside
terrain. Check pause/resume during a fall. Use `--no-save` for disposable sessions. Automated persistence fixtures use
unique temporary directories and preserve any existing user saves. CPU tests
cover malformed files and allocation/write/flush/sync/close/replace failures;
two-process graphical fixtures cover actual edit callbacks, F5, normal-exit
saves, and restored rendered chunks. Issue #9 tracks merged foundation delivery.
