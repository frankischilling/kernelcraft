# Sky depth rejection

The sky now renders after opaque terrain and before selection, clouds, and the
HUD. Covered pixels fail the depth test instead of running the sky shader before
terrain paints over them. The sky, cloud, and terrain shaders and all assets are
unchanged. The optimization retains the day/night cycle, stars, sun and moon
glow, terrain lighting, cloud motion, and wireframe rendering.
The hot mesh builder also receives a guarded 64-byte function-entry alignment
hint after a separate experiment identified sensitivity to executable layout.

In the final 1080p surface benchmark, completed render-frame mean fell from
**1.312 to 1.058 ms by day (19.4%)** and **1.411 to 1.085 ms at night (23.1%)**.
Sky GPU mean fell by 83.3% and 81.8%, respectively. These are medians of three
paired trial means on the Intel GPU below, not measured input latency or a
hardware-independent FPS claim.

This work is in [PR #69](https://github.com/frankischilling/kernelcraft/pull/69),
addressing [issue #68](https://github.com/frankischilling/kernelcraft/issues/68).
Its base branch contains the pending mesh and cloud changes from
[PR #65](https://github.com/frankischilling/kernelcraft/pull/65) and
[PR #67](https://github.com/frankischilling/kernelcraft/pull/67). Neither dependency
PR was merged to prepare this work. The new PR also remains unmerged.

## Review of changes since the cycle

`f41c36ecc307f36bb32f22554289dc3c87f5dbb3` introduced the cycle itself, so
its parent, `87675f8`, is the pre-cycle reference. The review included the
following production changes, their callers, and their tests:

| Changes | Finding and action |
| --- | --- |
| `f41c36e`: clock, sky, sun/moon/stars, terrain light uniforms | The full-screen sky draw ran before opaque terrain. Retained a measured change to depth-tested background rendering. Clock sampling is bounded scalar work; its pause and resume rules are intentional. |
| `dd0092b`, `9c66100`: chat, time commands, panels and input isolation | Parsing and history movement run on submission. Per-frame text fitting is bounded by 160-character messages, 128-character input storage and eight history rows. Measured empty, populated, open and narrow chat separately; did not rewrite command handling or claim a HUD speedup. |
| `1d647ac`, `9c66100`, `f3c00f3`, `03e0783`, `fee2b47`: gradients and celestial glow | These add real shader work. Depth rejection removes that work where terrain covers it without changing palette interpolation, angular glow, stars, or sprites. |
| `954b61e`: moving blocky clouds | Retained the original appearance and the later occupancy cache. The separate bounds improvement in PR #67 is already part of this baseline. |
| `dcf1185`: earlier performance pass | Retained inactive-palette and off-cone skips, cached cloud occupancy, hidden diagnostic behavior, and meshing improvements. These are existing optimizations, not new claims. |
| Pending mesh PR #65 | Retained its rectangle reuse and integer bounds. A separate alignment hint stabilizes the hot builder's entry alignment after executable layout affected measured edit cost. Editing still rebuilds only affected chunks, with unchanged geometry and upload counts. |

The terrain fragment lighting algorithm predates the cycle; the cycle changes
its uniforms. The review did not find a new per-frame allocation, unbounded
clock catch-up, command parser in the render loop, or minimized-window busy loop.
The redundant terrain-program save/bind/restore around light uniforms and bounded
chat fitting remain possible CPU follow-ups, not demonstrated causes of the sky
regression. Player simulation, selection, saving and world formats are unchanged.

## Implementation and correctness

`renderSky` enables `GL_LEQUAL`, sets the depth range to `(1, 1)`, and disables
depth writes for the existing full-screen triangle. Background depth is cleared
to 1. Opaque terrain is drawn first with the normal depth range, so covered sky
samples fail. The shared vertex shader still has the same projected coordinates
for clouds. Selection stays after the sky so translucent highlights at silhouettes
remain visible. Wireframe terrain leaves background between its lines.

The renderer restores the caller's depth-test enable, depth function, depth
range and write mask, as well as its existing program, VAO, texture, blend, cull
and polygon state. No new GL extension, heap allocation, texture, buffer, retained
cache or production profiling hook is needed. The only additional local state is
a depth function and two depth-range values; process memory was not measured.

`buildChunkMesh` has `__attribute__((aligned(64)))` under GCC/Clang, with the
unchanged C11 implementation for other compilers. This affects function-entry
placement, not the meshing algorithm or interface. It can add a small amount of
executable padding; there is no new runtime storage or alignment requirement on
world data. Build flags are unchanged. It does not guarantee identical loop
placement across compiler versions or a speedup on every processor.

The new tests compare 128 complete images byte for byte against the original
sky-first draw using the unchanged shader. They cover eight cycle phases,
eight viewpoints, filled and wireframe terrain, narrow/wide FOVs, portrait and
landscape views, selection and clouds below/inside/above the layer. The sky pass
also leaves every depth value unchanged. A separate covered-depth fixture must
return zero passing sky samples. That checks depth rejection; GPU timing supplies
the evidence of reduced shader cost. The initial foreground-pixel regression
failed before the production fix and passed afterward.

Existing independent palette, star, horizon, halo, terrain-lighting and cloud
assertions remain. All 112 cloud bounds image comparisons still pass. Real
application tests now obtain their unobstructed background reference in a test
framebuffer, retaining their terrain coverage and wireframe assertions after the
production order change. Caller-state tests include nondefault depth range and
function, and both initially enabled and disabled depth testing.

## Measurement method

Baseline production: `23ccb2857d1b8331375ca5731d4d70021fa0e8d5`, the combined
pending mesh/cloud branches. Final candidate:
`b52153a828c90d764b5ca4ba7a33f7d72cf0d88a`. The earlier sky-only version is
`09b7d19ce984724ae0d359e623de6a1558c87a28`. Later report commits do not change
the measured implementation.

Native Windows 11 Pro 10.0.22621, Intel Core i7-13700H, **Intel UHD Graphics**,
OpenGL 4.6.0 driver 32.0.101.7077; MinGW GCC 13.2.0, C11,
`-O2 -Wall -Wformat=2 -Wstrict-prototypes -Werror`. The installed NVIDIA adapter
was not the active renderer. Framebuffers are verified as 1280x720 or 1920x1080;
the additional narrow chat case uses 320x720. VSync is disabled in both builds,
matching the application's existing default.

The identical profiler uses seed 0, FOV 70, the same ten deterministic scenes,
120 warm-up frames and 600 measured frames per scene. Atmosphere and normal HUD
text are enabled. Phase advances from the explicit starting phase by
`step / 72000`; cloud offset is `step * 0.01`. Day starts at 0.125, night at
0.625, dawn at 0 and dusk at 0.5. Suites alternate baseline/candidate order.
Runs are serial, with no simultaneous builds or other benchmarks.

Full render-frame intervals include preparation between frames, CPU submission,
buffer swaps and completion of the final queued GPU batch. CPU submission is
measured separately around drawing and excludes the final drain. Whole-frame
elapsed queries and separate sky, terrain, cloud and HUD timestamps are read
after GPU completion, outside the measured frames. Sky/terrain/HUD CPU pass
timers exclude their two timestamp submissions; the existing cloud CPU timer
includes them. An empty pass still has timestamp overhead. Instrumentation
creates its query pool once and reuses it across scenes. No blocking query or
`glFinish` was added to the game loop.

These measurements cover the rendering path, not keyboard/mouse latency,
physics throughput, file I/O, displayed gameplay FPS or hands-on playtesting.
Per-trial p95/p99 values describe the measured batch intervals, including desktop
scheduling and queue completion; they are not a guarantee of displayed pacing.

The final comparison checks identical `render_benchmark.c`, `render_profile.h`,
`sky_render_checks.h` and `cloud_render_checks.h` hashes in both trees. The
baseline receives these profiling/test overlays after its original native tests
pass. `-BaseSkyFirst` selects the baseline's original production order; the
candidate uses terrain then sky. This order difference is the optimization under
test. Earlier diagnostic trials used differing correctness fixtures and are
labeled separately in the data, rather than substituted for final results.

## Investigating the editing regression

The first sky-only suites improved 1080p GPU throughput but increased the 720p
seam-edit mean. Five-pair repeats retained the increase. The reported rebuild
interval includes CPU construction, visibility/occluder construction, allocations,
OpenGL uploads and state setup; it is not an isolated mesh-computation timer.

Three controlled diagnostics narrowed this down:

1. Running the candidate binary with the original sky-first order retained the
   higher rebuild interval.
2. Disabling the atmosphere entirely, so no changed sky code initialized or ran,
   also retained it. These subsystem diagnostics are not acceptance results.
3. Linking the original baseline objects with 96 unexecuted text bytes after
   `sky.o` moved the unchanged renderer and mesh functions to the candidate's
   addresses. Across five alternating pairs, that padding alone raised rebuild
   means from 0.451–0.467 to 0.526–0.556 ms with atmosphere disabled.

The last experiment establishes executable-layout sensitivity without changing
any executed baseline code. It does not identify a particular instruction-cache
or branch-prediction mechanism. No diagnostic padding is included in the PR.

A separate candidate-only experiment recompiled only `mesh.c` with
`-falign-functions=64`, retaining all other objects and options. Five pairs
reduced the rebuild interval from 0.528–0.542 to 0.462–0.481 ms. The delivered
function attribute is narrower than that experimental flag: it applies only to
`buildChunkMesh`. The final benchmark places it at `0x14002b740`, matching the
experimental entry alignment; the application places it at `0x14001eb40`.
These are link-time addresses, before any loader relocation.
Final paired measurements below rerun the actual delivered attribute rather
than treating the flag experiment as acceptance evidence.

The trial data labels the earlier complete suites `depth-only`, the order and
layout experiments `diagnostic`, and the delivered revision's suites `final`.

## Historical diagnostic

Three alternating-order rounds at 1920x1080, surface view, 600 measured
frames per run. Values below are medians of trial means in milliseconds.
These revisions have different features and assets; this describes the
historical cost, not a visually equivalent optimization comparison. Absent
sky/cloud modules use no-op adapters in the profiling overlay. Production
code in each historical revision is unchanged. The first build-time runs
were serialized diagnostics and are not included in these pipelined values.

| Revision | Day frame | Night frame | Day sky GPU | Night sky GPU |
| --- | ---: | ---: | ---: | ---: |
| 87675f8: before cycle | 0.959 | 0.950 | n/a | n/a |
| f41c36e: initial cycle | 1.279 | 1.338 | 0.372 | 0.430 |
| fee2b47: sky refinements | 1.434 | 1.495 | 0.536 | 0.589 |
| 954b61e: initial clouds | 1.725 | 1.793 | 0.529 | 0.585 |
| 23ccb28: combined pending PRs | 1.259 | 1.362 | 0.259 | 0.326 |

The later sky effects and clouds increased GPU work. The previous optimizations
already recovered part of that cost. The new comparison below starts from
those pending improvements, not from the slower initial implementations.
Individual historical trials and pass measurements are in
[the historical CSV](sky-history-runs.csv).

## Final paired results

Final suites contain **254,400 measured frames**, including four five-pair
follow-ups. The original three-pair results and follow-ups are shown separately.

All values are milliseconds, baseline → candidate. Each cell is the median
of the corresponding per-trial metric, not a pooled percentile. Full suites
have three alternating pairs (1,800 measured frames per variant per scene).
Every paired diagnostic, final and follow-up trial is retained in
[the trial CSV](sky-performance-runs.csv);
[environment records](sky-performance-environment.json) include controls and
source hashes. Raw per-frame CSVs remain in the local suite output directories.

### Complete render-frame means

| Scene | Day 720p | Night 720p | Day 1080p | Night 1080p |
| --- | ---: | ---: | ---: | ---: |
| underground | 1.021 → 0.989 | 1.014 → 1.017 | 1.181 → 1.065 | 1.264 → 1.095 |
| surface_still | 0.615 → 0.529 | 0.667 → 0.532 | 1.312 → 1.058 | 1.411 → 1.085 |
| overview | 0.531 → 0.461 | 0.541 → 0.456 | 1.050 → 0.785 | 1.066 → 0.790 |
| sky | 0.415 → 0.428 | 0.476 → 0.466 | 0.760 → 0.752 | 0.961 → 0.958 |
| translate | 0.555 → 0.523 | 0.606 → 0.558 | 1.174 → 0.934 | 1.294 → 1.032 |
| rotate | 0.570 → 0.557 | 0.571 → 0.572 | 1.053 → 0.840 | 1.155 → 0.918 |
| seam_edits | 1.043 → 1.019 | 1.022 → 1.064 | 1.316 → 1.110 | 1.406 → 1.160 |
| sky_moving | 0.431 → 0.432 | 0.452 → 0.451 | 0.764 → 0.760 | 0.957 → 0.954 |
| wall_moving | 0.549 → 0.528 | 0.557 → 0.527 | 1.004 → 0.735 | 1.120 → 0.758 |
| cloud_layer | 0.443 → 0.437 | 0.434 → 0.435 | 0.711 → 0.749 | 0.809 → 0.797 |

### Surface view detail

| Metric | Day 720p | Night 720p | Day 1080p | Night 1080p |
| --- | ---: | ---: | ---: | ---: |
| frame_mean_ms | 0.615 → 0.529 | 0.667 → 0.532 | 1.312 → 1.058 | 1.411 → 1.085 |
| frame_median_ms | 0.447 → 0.423 | 0.446 → 0.420 | 0.572 → 0.510 | 0.555 → 0.539 |
| frame_p95_ms | 0.873 → 0.932 | 0.874 → 0.849 | 1.105 → 1.045 | 1.097 → 0.978 |
| frame_p99_ms | 1.552 → 1.451 | 1.387 → 1.475 | 2.003 → 1.518 | 1.557 → 1.642 |
| cpu_submit_mean_ms | 0.418 → 0.417 | 0.416 → 0.419 | 0.475 → 0.455 | 0.480 → 0.479 |
| gpu_mean_ms | 0.601 → 0.514 | 0.650 → 0.517 | 1.287 → 1.034 | 1.374 → 1.062 |
| sky_gpu_mean_ms | 0.118 → 0.018 | 0.152 → 0.026 | 0.270 → 0.045 | 0.341 → 0.062 |
| sky_cpu_mean_ms | 0.017 → 0.035 | 0.014 → 0.035 | 0.019 → 0.040 | 0.018 → 0.037 |
| terrain_gpu_mean_ms | 0.343 → 0.350 | 0.356 → 0.346 | 0.747 → 0.719 | 0.763 → 0.723 |
| cloud_gpu_mean_ms | 0.053 → 0.055 | 0.055 → 0.054 | 0.118 → 0.117 | 0.120 → 0.121 |
| hud_cpu_mean_ms | 0.325 → 0.327 | 0.324 → 0.332 | 0.363 → 0.346 | 0.367 → 0.373 |

### 1080p sky GPU work

| Scene | Day | Night |
| --- | ---: | ---: |
| underground | 0.281 → 0.027 | 0.355 → 0.027 |
| surface_still | 0.270 → 0.045 | 0.341 → 0.062 |
| overview | 0.280 → 0.045 | 0.293 → 0.045 |
| sky | 0.330 → 0.326 | 0.503 → 0.503 |
| translate | 0.270 → 0.046 | 0.344 → 0.069 |
| rotate | 0.276 → 0.090 | 0.349 → 0.140 |
| seam_edits | 0.271 → 0.043 | 0.345 → 0.060 |
| sky_moving | 0.333 → 0.330 | 0.499 → 0.501 |
| wall_moving | 0.266 → 0.020 | 0.346 → 0.020 |
| cloud_layer | 0.270 → 0.283 | 0.358 → 0.351 |

### Movement and editing tails at 1080p

| Scene | Day p95 | Day p99 | Night p95 | Night p99 |
| --- | ---: | ---: | ---: | ---: |
| translate | 1.069 → 1.063 | 1.620 → 1.706 | 1.051 → 1.080 | 1.684 → 1.495 |
| rotate | 1.115 → 1.114 | 1.729 → 1.817 | 1.133 → 1.024 | 1.964 → 1.638 |
| seam_edits | 1.808 → 1.726 | 2.450 → 2.213 | 1.828 → 1.759 | 2.406 → 2.368 |
| sky_moving | 0.858 → 0.850 | 1.412 → 1.306 | 0.783 → 0.845 | 1.109 → 1.306 |
| wall_moving | 1.088 → 1.034 | 1.526 → 1.748 | 1.069 → 1.031 | 1.695 → 1.625 |

### Transition phases and populated HUD

| Suite, surface view | Frame mean | Frame p95 | Frame p99 | Sky GPU mean | HUD CPU mean |
| --- | ---: | ---: | ---: | ---: | ---: |
| dawn1080 | 1.467 → 1.078 | 1.112 → 1.046 | 1.675 → 1.560 | 0.403 → 0.058 | 0.350 → 0.359 |
| dusk1080 | 1.554 → 1.112 | 1.071 → 1.162 | 1.658 → 1.734 | 0.468 → 0.075 | 0.356 → 0.371 |
| debug1080 | 1.507 → 1.369 | 2.236 → 2.206 | 3.130 → 2.682 | 0.351 → 0.063 | 0.784 → 0.786 |
| chat-history | 1.508 → 1.466 | 2.360 → 2.394 | 3.118 → 2.970 | 0.274 → 0.049 | 0.872 → 0.869 |
| chat-open | 1.435 → 1.266 | 2.086 → 1.978 | 2.697 → 2.513 | 0.279 → 0.050 | 0.699 → 0.663 |
| chat-narrow | 1.132 → 1.083 | 1.710 → 1.687 | 2.181 → 2.113 | 0.083 → 0.008 | 0.626 → 0.606 |

### Variation between trials

| Suite, surface view | Base frame-mean range | Candidate frame-mean range | Base sky-GPU-mean range | Candidate sky-GPU-mean range |
| --- | ---: | ---: | ---: | ---: |
| day720 | 0.613–0.626 | 0.527–0.536 | 0.117–0.120 | 0.018–0.018 |
| night720 | 0.660–0.675 | 0.520–0.555 | 0.148–0.153 | 0.025–0.027 |
| day1080 | 1.293–1.335 | 1.054–1.083 | 0.267–0.273 | 0.044–0.045 |
| night1080 | 1.407–1.413 | 1.078–1.092 | 0.339–0.342 | 0.061–0.063 |
| dawn1080 | 1.462–1.472 | 1.072–1.078 | 0.396–0.405 | 0.058–0.058 |
| dusk1080 | 1.536–1.557 | 1.105–1.112 | 0.464–0.474 | 0.075–0.075 |

### Seam edit work

| Suite | Build/upload interval mean | Chunks rebuilt/frame | Upload calls/frame | Upload bytes/frame |
| --- | ---: | ---: | ---: | ---: |
| day720 | 0.417 → 0.423 | 2 → 2 | 4 → 4 | 167664 → 167664 |
| night720 | 0.411 → 0.437 | 2 → 2 | 4 → 4 | 167664 → 167664 |
| day1080 | 0.449 → 0.449 | 2 → 2 | 4 → 4 | 167664 → 167664 |
| night1080 | 0.462 → 0.467 | 2 → 2 | 4 → 4 | 167664 → 167664 |

### Five-pair follow-ups

| Suite | Frame mean | p95 | p99 | Build/upload interval |
| --- | ---: | ---: | ---: | ---: |
| day1080-cloud-layer-repeat | 1.025 → 0.993 | 1.697 → 1.632 | 2.193 → 2.018 | 0.002 → 0.002 |
| night1080-sky-moving-repeat | 1.028 → 1.008 | 1.684 → 1.634 | 2.112 → 2.195 | 0.002 → 0.002 |
| night720-cloud-layer-repeat | 0.969 → 0.932 | 1.560 → 1.506 | 1.945 → 1.948 | 0.002 → 0.002 |
| night720-edits-repeat | 1.536 → 1.538 | 2.268 → 2.243 | 2.660 → 2.712 | 0.496 → 0.507 |

## Interpretation and remaining limits

The reduction is consistent in sky GPU work where terrain covers the background.
The 1080p surface trial-mean ranges do not overlap. Translation, rotation,
overview, wall motion and seam editing also improve completed 1080p throughput.
Fully exposed sky and inside-cloud views retain the full sky shader. Their small
differences are not evidence of less shader work. Sky CPU submission itself rises
by roughly 0.02 ms in the surface case; the retained benefit is predominantly GPU
work, while overall CPU submission remains similar.

The initial final day1080 cloud-layer mean rose from 0.711 to 0.749 ms; five
focused pairs reversed that direction, 1.025 to 0.993 ms. Night1080 moving-sky
p99 changed from 1.109 to 1.306 ms in the full suite and from 2.112 to 2.195 ms
in the follow-up. Night720 cloud-layer p99 changed from 1.077 to 1.265 ms and
then from 1.945 to 1.948 ms. Focused runs have different absolute timing from
the full suites, so they remain separate. These results do not establish faster
exposed-sky rendering or improved displayed pacing.

The alignment hint removes most of the sky-only build/upload penalty. In the
final full suites, the day720 edit frame is 1.043 → 1.019 ms and the night720
edit frame is 1.022 → 1.064 ms. The latter is effectively even in five focused
pairs, 1.536 → 1.538 ms. At 1080p both day and night editing improve total
throughput; geometry, two rebuilt chunks, four uploads and 167,664 uploaded
bytes per edit frame are unchanged. Unedited scenes still have zero recurring
terrain rebuilds/uploads. No faster meshing algorithm is claimed for this hint.

The [interval/hitch CSV](sky-performance-hitches.csv) retains every final run.
Across 127,200 measured intervals per variant, 156 baseline and 118 candidate
intervals exceeded 5 ms. Of these, 152 and 111 were terminal intervals containing
the deliberate GPU drain. Their maxima, 568.035 and 388.778 ms, reflect completion
of hundreds of queued hidden-window frames. The remaining intervals over 5 ms
numbered four and seven, with maxima of 6.668 and 9.805 ms. This supplies no basis
for claiming fewer gameplay hitches or lower input latency. All terminal work is
included in the reported completed-batch mean.

The setup intervals below come from the three day1080 runs per variant and
are medians. They are seeded benchmark setup, not complete application startup
or cold-storage measurements. The [setup CSV](sky-performance-setup.csv) includes
every final run. There is no startup or process-memory improvement claim.

| Setup metric | Baseline | Candidate |
| --- | ---: | ---: |
| Seeded generation, ms | 28.263 | 31.345 |
| Mesh construction/upload plus GPU completion, ms | 59.535 | 60.832 |
| Initial geometry upload bytes | 13,353,624 | 13,353,624 |
| CPU block storage bytes | 4,194,304 | 4,194,304 |

All 21 source and staged asset files match between the final builds. Remaining
opportunities are the fully visible sky shader and CPU-heavy populated HUD
submission. They need their own attribution and image/state checks; this work
does not reduce their visual quality or remove their features.

## Reproducing the paired runs

Use separate worktrees at the baseline and candidate revisions above. Copy the
four benchmark source/header files named above from the candidate to the
baseline, and use the candidate's comparison script. Build both Release
benchmarks with `KERNELCRAFT_RENDER_PROFILE=1`,
`KERNELCRAFT_PROFILE_ATMOSPHERE=1`, `KERNELCRAFT_PROFILE_PHASE=0.125`, and
`KERNELCRAFT_PROFILE_SCENE=surface_still`. Set
`KERNELCRAFT_PROFILE_SKY_FIRST=1` for the baseline's build-time benchmark run;
remove it for the candidate. Run `build.cmd -Benchmark` in each tree.

Then, from the candidate tree:

```powershell
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_PHASE = '0.125'
$env:KERNELCRAFT_PROFILE_WIDTH = '1920'
$env:KERNELCRAFT_PROFILE_HEIGHT = '1080'
$env:KERNELCRAFT_PROFILE_SCENE = $null
$env:KERNELCRAFT_PROFILE_DEBUG = $null
$env:KERNELCRAFT_PROFILE_CHAT = $null
$env:KERNELCRAFT_PROFILE_SKIP_TEXT = $null
.\tests\compare_render_profile.ps1 -BaseDirectory <baseline> `
    -CandidateDirectory <candidate> -OutputDirectory <new-output-directory> `
    -Pairs 3 -Pipelined -BaseSkyFirst
```

Repeat at 1280x720 and phases 0.625, 0 and 0.5 as described in the suite records.
For chat, set `KERNELCRAFT_PROFILE_SCENE=surface_still` and
`KERNELCRAFT_PROFILE_CHAT=history` or `open`. The populated fixture has eight
maximum-length messages; open chat also fills the input. Use
`KERNELCRAFT_PROFILE_DEBUG=1` for the F3 case. Restore temporary environment
settings afterward. The benchmark uses disposable seeded terrain and does not
read or write the user's saved world.

## Validation

Native Windows checks passed:

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
.\build.cmd -Benchmark
.\tests\test_build.ps1
```

Release and Debug checks were repeated after the alignment hint. The build
regression suite completed 22 build invocations, including its expected failure
cases; build/profiler integration was unchanged by the later source-only hint.
The final Release benchmark was rebuilt before the final paired runs. Baseline
native tests passed before its measurement overlays were applied; pre-cycle and
initial-cycle native tests also passed before their historical overlays.

The hidden application checks exercised walking, running/FOV, crouching,
jumping, collision, flight, timed breaking, placement, selection after edits,
F3/F4, chat typing and time commands, focus/capture changes, resizing,
minimize/restore, F5, save/restart, normal shutdown and failure cleanup.
Save files used disposable fixtures; the original working tree and real world
were preserved. CPU checks retain independent world/material/surface coverage,
seed, save, dirty-neighbor and failure assertions. Rendering checks retain
unchanged geometry, no idle terrain uploads and exactly two chunk rebuilds for
each seam edit.

Both GCC and Clang Linux jobs passed for the implementation in
[the pull-request CI run](https://github.com/frankischilling/kernelcraft/actions/runs/34559902513):

```sh
make -j2 CFLAGS='-O2 -g -Werror' all test
make -j2 CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' all test
make test-sanitize
make -j2 CFLAGS='-O2 -g -Werror' test-gl
make test-build  # GCC job
```

These jobs use Ubuntu 24.04 and Mesa software rendering. Their correctness
results are separate from the native Intel GPU timings. No native Windows
sanitizer run, hands-on interactive playtest, input-latency study or performance
measurement on the NVIDIA GPU was performed.
