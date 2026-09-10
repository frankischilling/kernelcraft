# F3 text rendering

Opening F3 used to submit a FreeGLUT bitmap draw for every character in every
diagnostic label. On the tested Intel UHD driver, that cost more than rendering
the terrain. The HUD now caches its existing Helvetica 10, 12, and 18 glyphs in
one texture during initialization and draws textured quads for each label.
Text, placement, font sizes, and diagnostic update frequency stay the same.

## Implementation and ownership

`HUDInit` calls `initText` after loading the hotbar icons. The cache is a
512 × 1536 RGBA8 texture: 3 MiB of GPU storage, plus 3,072 bytes of character
advances on the tested platform. FreeGLUT draws the glyphs directly into a
temporary framebuffer, which is deleted immediately. No font files or new
dependencies are needed. RGBA stores the coverage alpha used by the existing
compatibility texture environment; nearest filtering preserves the pixels.

`renderText` draws one group of quads per label. It retains the driver's initial
raster-position rounding with one state query per label, then reproduces the
original floating-point advances and newline handling. Ordinary frames do not
call `glutBitmapString`, allocate the cache, upload its pixels, or read pixels
back from the GPU. F3 toggles reuse the same cache.

Initialization saves and restores the relevant GL state. An incomplete font
framebuffer fails HUD initialization and releases both the partial cache and
the icons. `HUDCleanup` deletes the cache while the GL context is still current.
The renderer continues to require an OpenGL 3.3 compatibility context.

## Measurements

Measured September 9, 2026 local time (September 10 UTC), comparing main at
`faff40b0559cc915c98d6907815d6b431a7d394a` against this implementation.
Both executables used the same profiling header, including deterministically
changing FPS digits, and the same native Windows Release GCC 13.2 build (`-O2`).
The baseline received only the profiling header and its `renderText` linker
wrapper; its production renderer was unchanged.

Environment: Windows 11 Pro 22621, Core i7-13700H, Intel UHD Graphics,
driver 32.0.101.7077, OpenGL 4.6 compatibility. The machine also has an NVIDIA
GPU, but the benchmark's GL renderer identifies Intel UHD as the active device.
Each scene used seed 0, 1280 × 720, 120 warmup frames and 600 measured frames.
Five alternating baseline/candidate pairs ran with F3 off, followed by five
pairs with F3 on: 108,000 measured frames in total.

The table reports the median of five run means, in milliseconds per frame.
The final column is the median and full range of the five **paired** reductions
`100 × (1 - candidate / baseline)`. It need not equal the ratio of the two
separate medians. These are pipelined batch timings, including the final GPU
drain. Every pair improved total frame time in every scene.

| F3 | Scene | Before, ms | After, ms | Paired reduction: median (range) |
| --- | --- | ---: | ---: | ---: |
| Off | underground | 1.907 | 0.581 | 66.5% (64.4–73.1%) |
| Off | surface_still | 1.680 | 0.528 | 69.1% (67.6–69.9%) |
| Off | overview | 1.686 | 0.504 | 70.4% (68.4–73.3%) |
| Off | sky | 1.755 | 0.442 | 76.0% (72.9–79.8%) |
| Off | translate | 1.909 | 0.576 | 69.2% (67.8–89.3%) |
| Off | rotate | 1.785 | 0.633 | 65.6% (49.7–93.1%) |
| Off | seam_edits | 2.918 | 1.649 | 43.6% (31.8–56.2%) |
| Off | sky_moving | 1.597 | 0.434 | 71.8% (71.0–76.1%) |
| Off | wall_moving | 1.768 | 0.568 | 67.6% (56.3–69.9%) |
| On | underground | 4.426 | 1.058 | 79.0% (61.0–83.3%) |
| On | surface_still | 4.008 | 0.984 | 80.0% (59.9–83.7%) |
| On | overview | 3.786 | 1.218 | 67.1% (54.8–83.6%) |
| On | sky | 3.673 | 0.902 | 74.9% (35.0–83.8%) |
| On | translate | 4.223 | 0.882 | 78.7% (17.8–85.3%) |
| On | rotate | 4.321 | 1.047 | 75.3% (41.8–82.3%) |
| On | seam_edits | 6.494 | 2.097 | 61.8% (57.4–89.0%) |
| On | sky_moving | 4.065 | 0.773 | 80.5% (79.4–85.8%) |
| On | wall_moving | 4.979 | 0.958 | 77.1% (67.5–84.6%) |

The runs show substantial timing variation, especially in the F3-on series.
Tail latency did not improve in every pair: in F3-on pair 2, translate's p99
rose from 6.714 to 32.922 ms despite its lower mean. Overview, sky, and rotate
also had higher p99 values in that pair. The median of the five paired p99
reductions was positive in every scene, but the measurements do not establish
that occasional hitches are eliminated or identify the cause of that outlier.
No observations were removed. The off/on series ran at different times, so
their difference is descriptive, not a paired estimate of F3's incremental
cost. The surface scene's separate medians were 1.680/4.008 ms before and
0.528/0.984 ms after with F3 off/on respectively. The overlay still costs work;
the change removes the large per-character bitmap overhead.

An earlier single-scene diagnostic run measured 1.658 ms with F3 off, 3.494 ms
with F3 on, and 0.427 ms with F3 on while bypassing only text drawing. Terrain
draws (42) and triangles (22,890) were identical. That experiment isolated the
text path; it is separate from the repeated comparison above.

Terrain draws, triangles, exposed blocks, query counts, rebuilt chunks, upload
calls, and upload bytes matched between each baseline/candidate pair. Idle
scenes performed no mesh uploads; seam edits rebuilt two chunks per frame.
Per-run CPU submission, GPU elapsed, rebuild, p95 and p99 timings are retained
in [the data](benchmarks/f3-2026-09-09/runs.csv), with paired summaries in
[summary.csv](benchmarks/f3-2026-09-09/summary.csv). The
[manifest](benchmarks/f3-2026-09-09/manifest.json) records source and measurement
provenance; the archive contains every frame and process log.

These hidden rendering benchmarks exercise the real world renderer and HUD
with fixed scene scripts. They do not measure input, physics, visible window
presentation, or hands-on gameplay. GPU timer results can include command
queue starvation. Results from this driver are not an FPS guarantee for other
hardware.

## Reproduce

Build the baseline and candidate Release benchmarks with an identical copy of
`tests/render_profile.h`. Add `-Wl,--wrap=renderText` to the baseline benchmark's
link flags, as in the candidate. Run from the candidate checkout in PowerShell:

```powershell
$env:KERNELCRAFT_PROFILE_SKIP_TEXT = $null
$env:KERNELCRAFT_PROFILE_SCENE = $null
$env:KERNELCRAFT_PROFILE_DEBUG = $null
.\tests\compare_render_profile.ps1 -BaseDirectory <baseline> -CandidateDirectory <candidate> -OutputDirectory <new-off-directory> -Pairs 5 -Pipelined
$env:KERNELCRAFT_PROFILE_DEBUG = '1'
.\tests\compare_render_profile.ps1 -BaseDirectory <baseline> -CandidateDirectory <candidate> -OutputDirectory <new-on-directory> -Pairs 5 -Pipelined
$env:KERNELCRAFT_PROFILE_DEBUG = $null
```

`KERNELCRAFT_PROFILE_SCENE=surface_still` selects one scene for diagnosis.
`KERNELCRAFT_PROFILE_SKIP_TEXT=1` bypasses only text drawing in the benchmark.
These switches do not change the game. Leave the machine free of other builds
and graphical tests while measuring. The comparison script records the options
and rejects partial scene output. The archived main series predates that
metadata addition; each original log records its options in `PROFILE_HUD`.

## Regression coverage

The HUD test compares framebuffer pixels against the original FreeGLUT path
for all three fonts, every nonzero byte glyph, fractional positions, right
alignment, multiline text, empty text, and clipping. It also asserts that
ordinary HUD draws do not call the bitmap string renderer, while retaining the
existing layout, resize, icon, and GL-state checks. A simulated incomplete font
framebuffer checks cleanup and successful reinitialization. The bitmap-call
assertion failed against the original renderer before the fix.

Passed locally:

- Native Windows GCC: `.\build.cmd -Test`,
  `.\build.cmd -Configuration Debug -Test`, and `.\tests\test_build.ps1`.
- WSL Ubuntu GCC: `make -j4 all test`, `make test-sanitize`, `make test-gl`,
  and `make test-build`.
- WSL Ubuntu Clang: `make CC=clang CFLAGS='-O2 -g -Werror' all test` and
  `make CC=clang CFLAGS='-O2 -g -Werror' test-gl`.
- `clang-format --dry-run --Werror` on all changed C/H files.
- A one-pair filtered `surface_still` comparison after adding scene selection
  support to the comparison script; this check is outside the main dataset.

The application harness covers startup, HUD resizing and state restoration,
input/pause transitions, edits, saving, and process restart with temporary
world files. Graphical checks used hidden windows; no hands-on interactive
playtest was performed. WSL checks are separate from native Windows results.
