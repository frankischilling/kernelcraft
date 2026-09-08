# Shader startup failures

Base: `5cb431ae1a8304e1a75d9af0d2dca39f2fb44825`, after merging
[PR #19](https://github.com/frankischilling/kernelcraft/pull/19) as requested.
It was the only open PR. Its hosted checks and a fresh local
`make -j4 all test test-gl` passed before the merge. The working tree was
clean, and no applicable AGENTS.md was found. The ignored user save remains
untouched.

## Baseline and repair

The native C11/OpenGL game already has configurable Linux builds, a MinGW
Windows build, executable-relative assets, framebuffer-driven projection/HUD,
and cleanup before context destruction. CPU world queries, seeded terrain,
greedy chunk meshes, dirty seam updates, DDA editing, fixed-step player collision,
and versioned full-world saves have automated coverage. Four separate repeating
textures remain the runtime material path. The historical `atlast.py` utility
still uses an old input directory; it is outside this increment.

Shader loading did not check whether `glCreateShader` or `glCreateProgram`
returned zero. Later GL queries on those invalid handles left status and log
storage uninitialized. A real-context test reproduced invalid-handle GL errors
at all three creation stages, exiting 1. One run also printed an uninitialized
compilation diagnostic.

The loader now returns zero immediately with a stage-specific message when
creation fails, releasing any shaders already created. Status and log buffers
are initialized. `main` already treats a zero result as startup failure and
tears down the window/context before returning exit 1.

`tests/test_shader.c` substitutes only GLEW's creation function pointers.
Compilation, linking, deletion, and object-lifetime queries still use the real
driver. Tests cover vertex/fragment/program creation failures, invalid source
in either shader, a mismatched link interface, and successful loading afterward.
They check that each intended stage was reached, that failure returns zero,
that no invalid-handle GL error occurs, and that allocated objects are released.
They do not exhaust actual driver memory or simulate full GPU/context loss.

## Validation

These commands passed with exit 0:

```sh
make -s bin/linux/Release/test-shader
(cd bin/linux/Release && xvfb-run -a ./test-shader)
make -j4 CC=gcc CFLAGS='-O2 -g -Werror' all test test-gl
make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl
make -j4 CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' all test test-gl
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize
```

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
clang-format --dry-run --Werror src/graphics/shader.c tests/test_shader.c
```

The Release suites validated implementation commit `cc38bcf`. Review suggested
asserting that failures reach the intended stage; commit `4019c0c` adds those
test assertions. The targeted Linux shader test and full Linux/Windows Debug
suites passed with them. The sanitizer suite covers the unchanged CPU modules.
Compilation/link error messages from deliberately invalid shader fixtures are
expected; test exit status is zero after the repair.

Linux validation used Ubuntu under WSL, GCC 13.3.0, Clang 18.1.3, GLFW 3.3.10,
GLEW 2.2.0, FreeGLUT 3.4.0, and Mesa llvmpipe. Native Windows used MinGW64
GCC 13.2.0, GLFW 3.3.8, GLEW 2.2.0, and Intel UHD Graphics. These are real-context
scripted tests, including application navigation, collision, editing, affected
chunk rendering, and persistence across process restart.

An initial Debug validation attempt exited 2 because it ran alongside
`test-sanitize`; both write `obj/linux/Debug/build-settings.tmp`. This was a
validation scheduling error. After the sanitizer command finished, the full
Debug command passed separately. Run those two commands sequentially in a
shared checkout. No build change or suppressed failure was needed.

`timeout 8s xvfb-run -a make run` reached the normal executable and returned
the expected timeout status 124 when run through the temporary Bash validation
script. An earlier inline PowerShell/WSL attempt returned 1 after reaching the
executable and is not counted as a successful launch check. Clean shutdown is
covered by the application fixtures, not by the timeout command.

Linux logs are in `%TEMP%/kernelcraft-shader-{gcc,clang,debug-rerun,sanitize,launch}.log`;
the native Debug log is `%TEMP%/kernelcraft-shader-windows-debug.log`.
The Bash entry point is `%TEMP%/kernelcraft-shader-validation.sh`, invoked as
`wsl -d Ubuntu -- bash /mnt/c/Users/imike/AppData/Local/Temp/kernelcraft-shader-validation.sh STAGE`.
The existing user save retains SHA-256
`6e531f1eb6bc2dff821454184501e5db0cdd537acdecf790ee73484b6cf15e1d`.

No physical keyboard/mouse or monitor-scaling playtest was performed; native
desktop input controls are unavailable in this session. Windows sanitizers,
macOS, and a local rerun of `make test-build` were not performed. Build files
are unchanged, and the existing hosted GCC job includes incremental-build
checks. No performance comparison is claimed.

## Workflow and continuation

The three requested manifests were read at their installed paths:

- `C:/Users/imike/.codex/skills/humanizer/SKILL.md`
- `C:/Users/imike/.codex/skills/git-commit-author/SKILL.md`
- `C:/Users/imike/.codex/skills/git-human-workflow/SKILL.md`

Git/GitHub operations use the last skill's `scripts/git-human-workflow.ps1`
entry point and its Bash helper. Their supported commands were inspected,
as was `git-commit-author/scripts/git-commit-author.ps1`. Commit Author composes
through the workflow helper. The verified identity is Francis Hagan
`<frankhagan890@gmail.com>`; the active GitHub account is `frankischilling`.
Global configuration, published history, and existing hooks were preserved.

Work is on `fix/shader-creation-failures` under
[issue #20](https://github.com/frankischilling/kernelcraft/issues/20), with
implementation `cc38bcf` and test follow-up `4019c0c` in
[draft PR #21](https://github.com/frankischilling/kernelcraft/pull/21).
An independent coding-agent review found no blocking issue. This was not a
human approval. The new PR remains open for review; issue #20 closes on merge.

Build/run remains `make run` on Linux and `.\build.cmd -Run` on Windows.
Use the built executable with `--no-save` for disposable playtesting, or a
new explicit `--world` path to test F5, exit, and restart persistence.
The next milestone is physical foundation acceptance in
[issue #9](https://github.com/frankischilling/kernelcraft/issues/9): navigation,
wall/ceiling/corner collision, editing across chunk seams, save/restart,
capture/focus/minimize restoration, and monitor scaling. Trees, caves, streaming,
Fire Bugs, Goblins, and the Cupid Sponge remain planned after that foundation.
