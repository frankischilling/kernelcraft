# Terrain texture array

Base: `1258769cde309f7f722fb3e9578d2224baea6c9a`, after merging
[PR #21](https://github.com/frankischilling/kernelcraft/pull/21) as requested.
It was the only open PR. Its hosted checks and a fresh local
`make -j4 all test test-gl` passed before the merge. The working tree was clean,
no applicable AGENTS.md was found, and the ignored user save was preserved.

## Baseline and implementation

The baseline already implements C11/Make and MinGW builds, predictable assets,
framebuffer-driven projection/HUD, GL cleanup before context teardown, seeded
terrain, greedy meshes, DDA edits, dirty seam updates, fixed-step collision,
and versioned full-world saves. Automated application fixtures exercise those
systems; physical input and monitor-scaling acceptance remains incomplete.

The renderer still submitted separate stone, dirt, grass-top, and grass-side
batches. This increment completes shared runtime texture storage using a
four-layer `GL_TEXTURE_2D_ARRAY`. Each mesh vertex carries its face's material
layer, and each visible chunk submits its full index buffer in one terrain
draw. Material boundaries still constrain greedy merging. World generation,
save data, visibility, physics, and controls keep their existing behavior.

Array layers repeat independently, avoiding atlas tile borders while keeping
UVs proportional to a merged rectangle's dimensions. See the
[Khronos array texture reference](https://wikis.khronos.org/opengl/Array_Texture).
Storage uses `glTexImage3D`, available under the existing OpenGL 3.3 compatibility
requirement. Sampling uses nearest filtering, repeated S/T coordinates, and no
mipmaps. All layers must have matching dimensions. This uses the existing PNG
assets; the historical atlas image and `atlast.py` are outside the runtime path.

The loader validates paths, layer counts, and image dimensions. It converts
each image to RGBA, releases decoded pixels after upload, and deletes partial
GL storage on failure. CPU material batches remain useful for mesher coverage
checks; GPU draw submission uses the complete index count. The world renderer
owns the texture array and deletes it with the other GL resources.

## Correctness evidence

The new draw-count check failed on the baseline: the first benchmark view
submitted 31 terrain draws plus the grid for eight chunks. With the array it
submits eight terrain draws plus the grid. Every ordinary benchmark frame
checks one draw per visible chunk and no idle buffer uploads/uniform lookups.

CPU tests verify every rectangle's vertex layer alongside coverage, material,
winding, UV, negative-coordinate, and seam checks. Graphical tests compare
six views each of grass, dirt, stone, and mixed-material prisms with independent
unit-cube submissions. The reference uses a separate `sampler2D` shader and
independent face-to-texture mapping. All 24 comparisons matched under Mesa and
native Intel graphics within three channel levels, with 53,824 or 58,800 pixels
compared per view. The tolerance remains at most 0.2% differing pixels.

Temporarily changing the built fragment shader to sample stone for dirt made
58,800/58,800 bottom-face pixels differ and the benchmark exit 14. Restoring
the shader returned the benchmark to passing, including a run restricted to
Mesa OpenGL 3.3/GLSL 330. The mutation did not change source or saved worlds.

Loader fixtures verify RGBA pixels and layer order, dimensions, repeat/nearest
settings, null/missing/mismatched paths, invalid counts, storage failure, and
failure during either layer upload. GL lifetime queries verify cleanup.
These inject invalid API parameters to produce errors; they do not exhaust
driver memory or establish full context-loss recovery.

## Measurements

Three paired before/after runs used seed 0, the same four benchmark camera
views, 960x540 framebuffer, render distance, GCC 13.3.0 `-O2 -g` build, and
Mesa llvmpipe (LLVM 20.1.2, 256 bits) under Ubuntu/WSL. Each view warms up for
10 frames, then measures 60 frames with `glFinish`, selection, and F3 HUD.
Builds and other validation suites did not run alongside these measurements.
The before executable and assets were copied from the validated base.

| View pitch / camera height | Before median ms (range) | After median ms (range) | Terrain draws before / after |
| --- | --- | --- | --- |
| 0 degrees / 10 | 5.378 (4.677 to 5.708) | 4.926 (4.819 to 5.724) | 31 / 8 |
| -30 degrees / 10 | 4.853 (4.443 to 5.074) | 4.533 (4.522 to 5.383) | 31 / 8 |
| 89 degrees / 40 | 1.392 (1.283 to 1.537) | 1.767 (1.500 to 1.862) | 0 / 0 |
| -45 degrees / 32 | 4.802 (3.942 to 7.057) | 4.072 (4.056 to 4.920) | 31 / 8 |

The three terrain views still submit eight chunks, 2,929 quads, and 5,858
triangles; the sky view submits none. Every view adds one grid draw. Frame
times vary, ranges overlap, and the sky view regressed in these runs. These
measurements establish the draw reduction, not a general frame-rate gain.

Seed 0 still has 79,481 quads and 210,404 exposed unit faces. The material field
adds four bytes per vertex, raising vertex/index payload from 12,081,112 to
13,352,808 bytes (about 10.5%). These exclude driver bookkeeping. There is no
texture compression, mipmapping, or new rendering-distance feature here.

Raw paired results and the baseline executable/assets are under
`%TEMP%/kernelcraft-array-1258769/{before,after}-{1,2,3}.log`. An initial inline
WSL loop lost its loop-variable quoting and wrote one `baseline-.log`; it was
discarded from the comparison. The recorded pairs ran from a saved Bash script.

## Validation commands

These commands passed with exit 0 on implementation `1e0ca7c`:

```sh
make -j4 CC=gcc CFLAGS='-O2 -g -Werror' all test test-gl
make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl
make -j4 CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' all test test-gl
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize
(cd bin/linux/Release && MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 xvfb-run -a ./benchmark)
```

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
clang-format --dry-run --Werror src/graphics/texture.c src/graphics/texture.h src/graphics/world_renderer.c src/world/mesh.c src/world/mesh.h tests/test_shader.c tests/test_world.c tests/render_benchmark.c
```

Linux uses Ubuntu under WSL, GCC 13.3.0, Clang 18.1.3, GLFW 3.3.10,
GLEW 2.2.0, and FreeGLUT 3.4.0. Native Windows uses MinGW64 GCC 13.2.0,
GLFW 3.3.8, GLEW 2.2.0, and Intel UHD Graphics. Both native configurations
passed the same 24 material views, shader/texture failure fixtures, CPU tests,
and actual application startup, movement, collision, editing, mesh updates,
and process-restart persistence. Debug and CPU sanitizer builds ran sequentially
because they share the Linux Debug output directory.

Hosted GCC/Clang checks passed on the implementation commit, including
incremental-build checks. Build scripts are unchanged; `make test-build` was
not rerun locally. Windows sanitizers and macOS were not run. Driver/context
loss and physical high-DPI/input behavior remain unverified. Intentional error
diagnostics in failure fixtures are expected and do not indicate suite failure.

Validation runs use `%TEMP%/kernelcraft-array-validation.sh`, invoked through
`wsl -d Ubuntu -- bash /mnt/c/Users/imike/AppData/Local/Temp/kernelcraft-array-validation.sh STAGE`.
Stages are `gcc`, `clang`, `debug`, `sanitize`, `measure`, and `launch`.
Logs are `%TEMP%/kernelcraft-array-{gcc,clang,debug,sanitize,gl33,windows,windows-debug}.log`.
The existing user save still has SHA-256
`6e531f1eb6bc2dff821454184501e5db0cdd537acdecf790ee73484b6cf15e1d`.

After `make -j4 all` completed, `timeout 8s xvfb-run -a make run` reached
`minecraft_clone` and exited with the intentional timeout status 124, recorded
in `%TEMP%/kernelcraft-array-launch-rerun.log`. An earlier attempt timed out
during recompilation and is excluded. The timeout checks the documented launch
path; clean shutdown and restart are covered separately by application fixtures.

## Workflow and continuation

Implementation `1e0ca7c` is on `feat/terrain-texture-array` in
[draft PR #23](https://github.com/frankischilling/kernelcraft/pull/23), addressing
[issue #22](https://github.com/frankischilling/kernelcraft/issues/22).
An independent code review found no actionable issue. No human approval is
claimed. The new PR remains open for review; issue completion follows merge.

The requested manifests were read at:

- `C:/Users/imike/.codex/skills/humanizer/SKILL.md`
- `C:/Users/imike/.codex/skills/git-commit-author/SKILL.md`
- `C:/Users/imike/.codex/skills/git-human-workflow/SKILL.md`

Git/GitHub operations use the last skill's `scripts/git-human-workflow.ps1`
and bundled Bash helper, with their supported commands inspected. The Commit
Author PowerShell helper was also inspected; identity enforcement composes
through Git Human Workflow. The verified author/committer is Francis Hagan
`<frankhagan890@gmail.com>` and the active account is `frankischilling`.
Global configuration, existing hooks, licenses, and published history were
preserved. No new assets were downloaded.

Build/run remains `make run` on Linux or `.\build.cmd -Run` on Windows.
Use `--no-save` for disposable navigation, or a new explicit `--world` path
to test F5 and restart persistence. F3 shows terrain draw counts; select each
material with 1/2/3 and place/break blocks across chunk seams. Grass uses its
top tile above, side tile laterally, and dirt below.

The next milestone is physical foundation acceptance in
[issue #9](https://github.com/frankischilling/kernelcraft/issues/9): navigation,
collision at walls/ceilings/corners, seam edits, capture/focus/minimize restore,
monitor scaling, and save/restart. Native desktop input controls are unavailable
in this session, so those manual checks remain unverified. Trees, caves,
streaming, Fire Bugs, Goblins, and the Cupid Sponge remain planned afterward.
