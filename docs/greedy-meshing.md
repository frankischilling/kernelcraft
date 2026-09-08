# Greedy chunk meshing

PR #15 is merged. See [texture-array integration](texture-array.md) for the
current material path. The measurements below describe the earlier meshing
increment with separate texture batches.

Work starts at `aab7744475bc0318149d89442280fed1cd3fcfcb`, after merging
PR #13. No other PR was open. The working directory was clean; the existing
ignored world save was preserved. Issue #14 tracks this increment on
`feat/greedy-chunk-meshing`.

## Baseline and implementation

The baseline already has configurable Linux and native Windows builds, startup
and GL lifetime checks, CPU world data, exposed-face chunk meshes, DDA editing,
player collision, deterministic seeds, and validated persistence. The command
`make -j4 all test test-gl` passed in Ubuntu under WSL before the merge,
including actual application startup, movement, editing, and process restart
fixtures. Its tree is identical to the merge base above. Interactive desktop
playtesting remains unverified.

The mesher now sweeps each oriented chunk slice and joins rectangles with the
same face material. It counts rectangles before allocating exact vertex/index
buffers, then repeats the deterministic sweep to fill material batches. It
retains the existing block query and dirty-neighbor rules. Empty meshes and
allocation failure cleanup keep the previous ownership contract.

Coordinates, bounds, outward winding, and face normals remain unchanged. UVs
span the rectangle's block dimensions; four separate textures use explicit
`GL_REPEAT` and nearest filtering. This avoids atlas bleeding and preserves
the grass top/side/dirt-bottom mapping. The unused atlas helper remains outside
the runtime pipeline. Lighting is evaluated per fragment from world position
and the constant face normal. Future vertex lighting or ambient occlusion must
extend the compatibility key before merging differently lit faces.

The HUD and renderer statistics call merged rectangles **quads**. A quad is
two triangles and may cover many unit block faces. Surface-block counts and
terrain draw counts retain their original meaning. This change reduces mesh
storage and triangles; it does not change the material batching scheme.

## Initial validation

- The new solid-chunk test failed against the previous mesher, then passed:
  4,608 unit faces become six quads.
- `make -j4 all test test-gl` passed after implementation and formatting.
- CPU coverage checks expand every rectangle into unit faces and compare
  against block queries. They check duplicates, omissions, internal faces,
  materials, UV orientation/scale, bounds, and winding on generated terrain,
  negative chunks, seams, prisms, holes, stairs, and mixed materials.
- Six graphical views compare a merged grass prism with independent unit-cube
  submissions. All 53,824 to 58,800 compared pixels per view matched within
  three channel levels under Mesa. Deliberately removing UV scaling made
  41,300/53,824 pixels differ and failed the fixture with exit 14. Restoring
  scaling returned the full suite to passing.
- Seed 0 retains 210,404 exposed unit faces. It now uses 79,481 quads and
  12,081,112 mesh bytes, versus 210,404 quads and 31,981,408 bytes before.
  These count vertex/index payloads, excluding driver bookkeeping.

An independent read-only code review found no blocking production defect. Its
suggestion to validate each quad's actual index coverage was added: indices
must reference that rectangle, use all four corners, and share a diagonal.
An isolated mesher mutation that duplicates the first triangle now fails these
checks. Production geometry was unchanged by this test addition.

## Workflow and skills

The requested installed manifests were read from:

- `C:\Users\imike\.codex\skills\humanizer\SKILL.md`
- `C:\Users\imike\.codex\skills\git-commit-author\SKILL.md`
- `C:\Users\imike\.codex\skills\git-human-workflow\SKILL.md`

Hosted changes and commits used
`C:\Users\imike\.codex\skills\git-human-workflow\scripts\git-human-workflow.ps1`
and its bundled `.sh` implementation. The resolved author/committer was Francis
Hagan <frankhagan890@gmail.com>; the authenticated GitHub account was
frankischilling with ADMIN access. No global configuration or published history
was changed, and no branches were deleted. Humanizer was used for public prose.
Design, test-first, verification, and review used the installed Superpowers
6.3.0 skills.

[PR #13](https://github.com/frankischilling/kernelcraft/pull/13) was merged first.
The implementation checkpoint is `57c48ae` on `feat/greedy-chunk-meshing`, in
[draft PR #15](https://github.com/frankischilling/kernelcraft/pull/15), linked to
[issue #14](https://github.com/frankischilling/kernelcraft/issues/14) and the
larger [foundation issue #9](https://github.com/frankischilling/kernelcraft/issues/9).
The new PR remains unmerged; issue completion follows merged delivery.

## Final validation on September 8, 2026

All positive commands below exited 0. Linux commands ran in Ubuntu under WSL,
from the repository. Native Windows commands ran in PowerShell.

| Command | Result |
| --- | --- |
| `make -j4 CC=gcc CFLAGS='-O2 -g -Werror' all test test-gl` | Release build, CPU, and Mesa application tests passed |
| `make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl` | Release build, CPU, and Mesa application tests passed |
| `make -j4 CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' all test test-gl` | Debug build, CPU, and Mesa application tests passed |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize` | CPU AddressSanitizer and UBSan passed |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=gcc test-sanitize` | CPU AddressSanitizer and UBSan passed |
| `.\build.cmd -Test` | Native Windows Release CPU, application, restart, and graphics checks passed |
| `.\build.cmd -Configuration Debug -Test` | Native Windows Debug checks passed, including the added index validation |
| `git diff --check` | Passed |

Linux versions: GCC 13.3.0, Clang 18.1.3, GLFW 3.3.10, GLEW 2.2.0,
freeglut 3.4.0, and Mesa llvmpipe (LLVM 20.1.2). Windows used MSYS2 MINGW64
GCC 13.2.0 and Intel UHD Graphics driver 32.0.101.7077. The six repeated-texture
views passed on both renderers with zero pixels differing by more than three
channel levels. Windows tests used the native driver, not cross-compilation.

The existing GCC/Clang Actions checks passed for checkpoint `57c48ae` on both
the [push](https://github.com/frankischilling/kernelcraft/actions/runs/34266894044)
and [PR](https://github.com/frankischilling/kernelcraft/actions/runs/34266968304).
They include incremental-build regressions; `make test-build` was not rerun
locally because the build scripts were unchanged. Final-head results are on
the PR checks tab.

Development failures were understood and corrected: a PowerShell/WSL quoting
error and CRLF in an ignored validation launcher prevented those invocations
from running as intended. Validation was rerun through an LF shell script,
and compiler flags were checked in the build-settings files. The intentional
solid-chunk, stretched-texture, and duplicated-triangle failures described
above demonstrate the regressions; they are not unresolved failures.

## Performance comparison

Three sequential before/after pairs ran after other validation finished. Both
executables use GCC 13.3.0, C11, `-O2 -g`, seed 0, the same four camera views,
960 x 540 resolution, render distance, and Mesa llvmpipe. The new build also
uses `-Werror`, which does not change generated code. The baseline executable
was retained from the pre-edit build at the merge-base tree. Each view warms
up for 10 frames and measures 60, including HUD, grid, and `glFinish`. The new
HUD calls the counter quads; the rest of the benchmark scene is unchanged.

| Camera view | Before median ms/frame | After median ms/frame | Terrain + grid draws, both |
| --- | ---: | ---: | ---: |
| Initial position, level | 6.047 | 4.582 | 32 |
| Initial position, 30 degrees down | 5.613 | 4.373 | 32 |
| Height 40, 89 degrees up | 1.277 | 1.282 | 1 |
| Height 32, 45 degrees down | 5.043 | 3.928 | 32 |

All views use X=0, Z=3, yaw=90 degrees; initial height is 10. Terrain views
submit eight chunks and 4,385 surface blocks. Submitted quads drop from 6,668
to 2,929, and triangles from 13,336 to 5,858. Ordinary-frame buffer uploads and
uniform-name lookups remain zero. Whole-world vertex/index payload decreases
62.2%, from about 30.5 MiB to 11.5 MiB. Median initialization increases from
97.965 to 165.809 ms because rectangle construction adds CPU work before drawing.
These are software-renderer measurements, not a hardware FPS guarantee.

To reproduce, build the base and branch in separate checkouts with
`make CC=gcc CFLAGS='-O2 -g' all bin/linux/Release/benchmark`. In each checkout,
run `cd bin/linux/Release` then `xvfb-run -a ./benchmark` three times, alternating
revisions without concurrent builds. Append a capture prefix to write PPMs.

Captured terrain views were visually inspected. In the region X=400..959,
Y=0..419, excluding HUD and material slots, the four image pairs had 25, 0, 0,
and 3 pixels differing by more than three channel levels out of 235,200 each.
Sparse differences are consistent with nearest sampling after retriangulation;
the explicit six-face prism regression independently checks texture repetition.

## Run and continue

Use `make run` on Linux or `.\build.cmd -Run` on native Windows. For disposable
playtesting, launch the built executable with `--no-save`. W/A/S/D moves,
Space jumps, F toggles flight, Escape releases/captures the mouse, 1/2/3 selects
material, and left/right click breaks/places. Use an explicit `--world` path
instead of `--no-save` to test F5 and persistence across a clean restart.

This completes the tested greedy-meshing increment on the branch. Physical
keyboard/mouse playtesting, desktop focus/minimize/resize and high-DPI behavior,
Windows sanitizers, and macOS remain unverified. The full playable-foundation
milestone is not declared complete. Next, finish interactive acceptance for
issue #9 on a disposable seeded world, including chunk-seam edits, jumping into
walls/ceilings/corners, and save/restart; record any reproducible defects before
adding trees, caves, streaming, or gameplay progression. Startup mesh CPU cost
is a measured optimization candidate. Fire Bugs, Goblins, and the Cupid Sponge
remain planned, with the existing C/OpenGL and Cube World direction preserved.
