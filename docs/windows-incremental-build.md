# Windows incremental build checkpoint

Base: `eb327a211bad4d469f6c1ec5979c3b0fc00a50c8`, after merging
[PR #23](https://github.com/frankischilling/kernelcraft/pull/23) as requested.
It was the only open PR. Its hosted checks and a fresh local
`make -j4 all test test-gl` passed before merging. The checkout was clean,
and no applicable AGENTS.md was found.

Implementation: `b1159ab2e5e58c505158083a870a4ad5f0fd7621` on
`feat/windows-incremental-build`, in
[draft PR #25](https://github.com/frankischilling/kernelcraft/pull/25), addressing
[issue #24](https://github.com/frankischilling/kernelcraft/issues/24).

## Behavior

The native Windows script previously compiled every game source on each build
and compiled shared sources again for each test executable. It now compiles
each translation unit once into `obj/windows/<configuration>`, sharing those
objects across executables and later invocations. GCC dependency files include
project and system headers. A compiler/settings signature tracks the driver,
C frontend, build script, flags, search environment, and working directory.
Missing dependencies or damaged metadata cause a rebuild. Failed compilation
leaves the previous object intact and stops the build.

Executables are always linked, and assets/runtime DLLs are always staged.
Release and Debug use separate output and object directories; `-Clean` removes
only the chosen configuration after checking its paths. Cache checks use file
modification times and assume serial builds per configuration. Clean after
restoring inputs with preserved timestamps or changing assembler/specs in place.
See [Windows setup](windows.md) for the complete invocation and cache rules.

The game, rendering, save format, and controls retain their existing behavior.
The baseline foundation and texture-array work are described in
[the preceding checkpoint](texture-array.md).

## Validation

These commands passed locally:

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
.\build.cmd -Test
& 'C:\msys64\usr\bin\make.exe' test-build
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/imike/kernelcraft && make test-build'
```

Native Windows used PowerShell 5.1, MinGW64 GCC 13.2.0, GLFW 3.3.8, GLEW 2.2.0,
FreeGLUT 3.4.0, and Intel UHD Graphics. Both configurations passed CPU tests,
shader/texture failure checks, HUD layout, actual application input/collision/
editing/framebuffer/shutdown fixtures, process-restart persistence, and render
checks. Each configuration compiled 32 source files once. The repeated Release
suite compiled zero files and passed again. Intentional failure diagnostics
are expected within these suites.

The Windows build regression executes 22 child builds plus an in-process build
in a temporary project copy containing spaces. It checks selective source/header
invalidation, object reuse, missing objects/dependencies, corrupt metadata,
compiler flag/search-environment changes, failed compilation and recovery,
configuration isolation, and cleaning. Relative `CPATH` tests select different
older headers from two caller directories and also check reuse after
`Push-Location` when PowerShell and .NET directories differ.

The initial regression failed against the baseline because no object cache
existed. A read-only review then found stale reuse with relative include paths.
The new reproduction failed before the working-directory fix, passed afterward,
and the reviewer verified the correction with no further findings. No human
approval is claimed.

One observed game-only cold build in the final regression took 11.13 seconds;
its unchanged repeat took 2.77 seconds on the same installation. These are
single observations, not a general performance guarantee. The object-count and
invalidation assertions provide the repeatable acceptance criteria.

Linux `make test-build` passed under WSL Ubuntu with GCC 13.3.0 and Clang 18.1.3.
Only the Windows Makefile branch changed. Local sanitizers were not rerun for
this PowerShell change; the existing hosted GCC/Clang workflow still runs
Release/Debug, CPU sanitizers, Mesa graphics checks, and Linux build regressions.
UCRT64, macOS, and native Windows sanitizers were not tested here.

The [hosted GCC/Clang workflow](https://github.com/frankischilling/kernelcraft/actions/runs/34285686496)
passed on implementation commit `b1159ab2e5e58c505158083a870a4ad5f0fd7621`,
including CPU sanitizers, Mesa checks, and the Linux build regression.

Logs are in `%TEMP%/kernelcraft-windows-cache-{build-final,release-final,debug-final,release-warm,linux-build}.log`.
The pre-fix reproduction is in `%TEMP%/kernelcraft-windows-cache-relative-red.log`.
The existing user save changed during the session before the first native suite
started. Its subsequently observed SHA-256 remained
`f0cda345d7cbb1d967a8917bc5585a8119ea32be0035d8b8b37edeb27d9103d4`
through the remaining checks; the current file was preserved.

## Workflow and next milestone

The requested manifests were read at:

- `C:/Users/imike/.codex/skills/humanizer/SKILL.md`
- `C:/Users/imike/.codex/skills/git-commit-author/SKILL.md`
- `C:/Users/imike/.codex/skills/git-human-workflow/SKILL.md`

Git/GitHub operations used
`C:/Users/imike/.codex/skills/git-human-workflow/scripts/git-human-workflow.ps1`
and its bundled Bash helper. Their supported commands and the Commit Author
helper were inspected; identity enforcement composed through Git Human Workflow.
The verified author/committer is Francis Hagan `<frankhagan890@gmail.com>`,
with active GitHub account `frankischilling`. Published history, global settings,
hooks, licenses, and attribution were preserved.

Build and run with `.\build.cmd -Run`; use a new explicit `--world` path or
`--no-save` for disposable gameplay checks. The next milestone remains physical
foundation acceptance in [issue #9](https://github.com/frankischilling/kernelcraft/issues/9):
navigation, walls/ceilings/corners, seam edits, focus/capture/minimize restore,
monitor scaling, and save/restart. The Computer Use skill at
`C:/Users/imike/.codex/plugins/cache/openai-bundled/computer-use/26.901.41600/skills/computer-use/SKILL.md`
was read with its API guidance, but its required `node_repl` entry point was
unavailable in this session. Those physical checks remain unverified.
Trees, caves, streaming, Fire Bugs, Goblins, and the Cupid Sponge remain planned.
