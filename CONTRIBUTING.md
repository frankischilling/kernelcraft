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
for dependencies and `docs/status.md` for the current continuation point.

Add tests for observable defects and boundary cases. CPU tests must not create
an OpenGL context. The application smoke harness substitutes GLFW event/input
delivery and framebuffer sizes while running the actual game loop and OpenGL
renderer. It cannot establish physical monitor scaling, window-manager behavior,
or whether controls feel right during interactive play.

For manual input checks: launch the game, navigate with W/A/S/D, release the
cursor with Escape, and confirm mouse/keyboard movement stops. Capture again and
check for a jump, then switch applications and return. Focus loss releases the
cursor; Escape resumes capture. Resize, minimize, and restore the window.
Report the platform, driver, and what you observed. Editing, collision, and
saves remain separate work in issue #9.
