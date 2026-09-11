# Cloud rendering bounds

The cloud pass now skips views facing entirely away from the cloud layer and
scissors partial views to a conservative screen rectangle. The ray-marching
shader, cached occupancy pattern, color, opacity, depth, drift, and fade distance
are unchanged. This reduces cloud GPU work while preserving the rendered image.

The existing occupancy cache was introduced in PR #60; it is not a new change in
this pass. This work is independent of the mesh rebuild changes in PR #65.

## Implementation and correctness

Outside the layer at Y=120 through Y=124, a ray must point toward the layer to
reach a cloud. Before normalization, its vertical component is linear in screen
coordinates: `front.y + right.y * scale.x * x + up.y * scale.y * y`.
The maximum over either screen axis gives a conservative bound on the other.
This includes camera roll. A float-arithmetic margin and one extra pixel expand
the rectangle. The original full-screen triangle and its interpolated values
remain unchanged.

An empty angular bound returns before querying or changing GL state. Partial
bounds use the current viewport and intersect an enabled caller scissor. Both
the scissor box and enable state are restored. Integer intersections use 64-bit
arithmetic. Views within or exactly on the layer boundaries retain the full
pass. Invalid coefficients fall back to the full pass.

There are no new heap allocations, persistent caches, textures, buffers, or GPU
resources in the game. The additional bounds use automatic local storage.
OpenGL 3.3 compatibility remains sufficient. The existing 512-byte occupancy
bitset and renderer lifecycle remain in use.

The rendering tests compare 112 views byte for byte against a direct,
unrestricted draw of the same unchanged shader. Cases cover heights below,
inside, above, and immediately beside both layer boundaries; nearly horizontal
rays; narrow and wide FOVs; camera roll; portrait and landscape viewports;
positive and negative viewport origins; and disabled, enabled, empty, and
disjoint caller scissors. Viewport and scissor restoration are checked alongside
the pixels. A separate work-count assertion requires no cloud draw when facing
entirely away from the layer; it failed on the original renderer before the fix.
Existing coverage, occupancy fingerprint, lighting, parallax, drift, depth,
inside-cell, pause, and GL state assertions remain in place.

## Measurement method

Baseline production revision: `a1deaf43028c75e0a5a7a15520255514dfa83697`.
Candidate code and tests: `40bbd7acccf27fa04b5ecff1b3e71352ac6150cd`.
The later report commit does not change the measured code.

Measured September 10, 2026, on native Windows 11 Pro 10.0.22621, an Intel
Core i7-13700H, and **Intel UHD Graphics**, OpenGL 4.6.0 driver 32.0.101.7077.
The installed NVIDIA adapter was not the active OpenGL renderer. Both builds
used MinGW GCC 13.2.0, C11, `-O2 -Wall -Wformat=2 -Wstrict-prototypes -Werror`.

The existing profiler renders seed 0 with FOV 70, sky, lit terrain, moving
clouds, selection, and normal HUD text enabled. Its ten deterministic scenes
include translation, rotation, seam edits, motion behind a wall, looking skyward,
and movement inside the cloud layer. Day starts at phase 0.125 and night at
0.625. Additional surface tests start at dawn (0) and dusk (0.5), with F3 enabled
for dusk. The phase advances by `step / 72000`; cloud offset is `step * 0.01`.
Each scene warms up for 120 frames and measures 600. Each pair alternates order;
all trials are retained.

Framebuffer sizes are verified as 1280x720 and 1920x1080, rather than inferred
from the requested window size. The hidden profiler window is undecorated so a
1080-high client area fits a 1080-high desktop. VSync is disabled in both builds,
matching the current application's swap interval. These are rendering-path
measurements, not input latency, simulation, file I/O, or displayed gameplay FPS.

Full-frame intervals include CPU preparation, submission, buffer swaps, and
completion of the final queued GPU batch. CPU submission excludes that final
drain. Whole-frame GPU elapsed queries and the new cloud timestamp pairs are
read after completion, outside measured frames. `cloud_cpu_ms` includes the two
timestamp submissions as well as `renderClouds`; it is instrumented submission
time. An empty cloud pass still measures the small timestamp interval. No query
readback or profiling synchronization was added to the normal game loop.

The first diagnostic batches used identical profiling code but different
correctness fixtures in the benchmark executable. Final comparisons compile
identical `render_benchmark.c`, `render_profile.h`, and `cloud_render_checks.h`
into both builds; the comparison script now checks all three hashes. Both the
diagnostic trials and final trials are preserved and labeled separately in
[the trial CSV](cloud-performance-runs.csv). The tables below use final trials.
[Environment records](cloud-performance-environment.json) contain suite controls,
driver information, timestamps, and source hashes.

## Results

The final 1080p day surface scene reduced cloud GPU mean from **0.248 to
0.113 ms (54.5%)** and full-frame mean from **1.594 to 1.391 ms (12.7%)**.
At night, the same cloud pass fell from 0.241 to 0.125 ms (47.9%). Level views
generally halve the cloud pass. Looking away removes the cloud draw entirely;
the roughly 0.001 ms remaining cloud interval is timestamp overhead.

All numbers below are milliseconds, baseline → candidate. Each cell is the
median of the corresponding per-trial metric, not a percentile pooled across
trials. Main suites use three alternating pairs (1,800 measured frames per
variant per scene). The two focused follow-ups use five pairs (3,000 per
variant). Final suites contain 127,200 measured frames in total. The range
tables retain run-to-run variation; the CSV includes every individual trial.

The improvement is clearest in cloud GPU work and in GPU-bound 1080p scenes.
Frame means and tails do not improve uniformly. The 720p translation p99 rose
from 2.0465 to 2.7954 ms in the full suite; five focused pairs reversed that
direction (2.492 to 2.400 ms). The 1080p wall-motion p95 rose from 1.456 to
2.039 ms in the full suite and fell from 2.267 to 1.896 ms in the follow-up.
These overlapping, changing distributions do not establish smoother movement.
Skyward and inside-layer views retain the original full draw; observed frame
changes there are not evidence of reduced cloud work.

The first diagnostic batches also showed varying tail regressions, including
CPU submission changes in unchanged full-cloud views. They prompted repeated
runs and the stricter identical-fixture check. They remain in the CSV under
`diagnostic-*`; none was removed for being slow. Even final matching binaries
showed substantial variation: one 1080p day seam-edit candidate trial averaged
2.884 ms/frame versus 1.842 and 1.926 ms in its other two trials. No universal
FPS or input-latency improvement is claimed.

### 1920x1080 day

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| underground | 0.231 → 0.113 | 1.351 → 1.188 | 0.941 → 0.850 | 1.945 → 1.982 | 2.868 → 2.979 |
| surface_still | 0.248 → 0.113 | 1.594 → 1.391 | 0.721 → 0.787 | 1.680 → 1.685 | 2.361 → 2.299 |
| overview | 0.201 → 0.001 | 1.369 → 1.120 | 0.712 → 0.616 | 1.837 → 1.599 | 2.396 → 2.346 |
| sky | 0.276 → 0.270 | 0.864 → 0.777 | 0.668 → 0.464 | 1.782 → 1.313 | 2.373 → 1.992 |
| translate | 0.244 → 0.118 | 1.452 → 1.231 | 0.711 → 0.671 | 1.533 → 1.622 | 2.347 → 2.578 |
| rotate | 0.239 → 0.116 | 1.327 → 1.175 | 0.921 → 0.715 | 2.391 → 1.806 | 3.083 → 2.713 |
| seam_edits | 0.249 → 0.131 | 1.962 → 1.926 | 1.719 → 1.810 | 3.421 → 3.020 | 4.387 → 3.949 |
| sky_moving | 0.274 → 0.275 | 0.915 → 0.791 | 0.589 → 0.504 | 2.245 → 1.339 | 3.681 → 1.956 |
| wall_moving | 0.227 → 0.110 | 1.183 → 1.177 | 0.643 → 0.932 | 1.456 → 2.039 | 2.280 → 2.703 |
| cloud_layer | 0.281 → 0.280 | 0.935 → 0.736 | 0.663 → 0.488 | 1.597 → 1.286 | 2.536 → 2.154 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| underground | 1.286–1.474 | 1.165–1.315 | 0.225–0.231 | 0.112–0.113 |
| surface_still | 1.459–1.638 | 1.351–1.608 | 0.234–0.254 | 0.112–0.129 |
| overview | 1.266–1.376 | 1.094–1.265 | 0.196–0.211 | 0.001–0.001 |
| sky | 0.765–1.407 | 0.771–0.786 | 0.267–0.458 | 0.268–0.270 |
| translate | 1.337–1.500 | 1.221–1.291 | 0.225–0.252 | 0.118–0.129 |
| rotate | 1.208–1.345 | 1.087–1.227 | 0.228–0.243 | 0.115–0.117 |
| seam_edits | 1.732–2.083 | 1.842–2.884 | 0.222–0.258 | 0.109–0.284 |
| sky_moving | 0.761–0.944 | 0.781–0.963 | 0.268–0.274 | 0.271–0.323 |
| wall_moving | 1.150–1.307 | 1.041–1.215 | 0.223–0.229 | 0.110–0.112 |
| cloud_layer | 0.718–1.277 | 0.711–0.897 | 0.274–0.424 | 0.280–0.287 |

### 1280x720 day

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| underground | 0.109 → 0.062 | 0.885 → 0.894 | 0.717 → 0.722 | 1.698 → 1.792 | 2.391 → 2.404 |
| surface_still | 0.107 → 0.055 | 0.766 → 0.668 | 0.559 → 0.524 | 1.536 → 1.381 | 2.179 → 1.907 |
| overview | 0.086 → 0.001 | 0.723 → 0.759 | 0.581 → 0.576 | 1.482 → 1.563 | 2.043 → 2.420 |
| sky | 0.153 → 0.130 | 0.642 → 0.585 | 0.534 → 0.464 | 1.284 → 1.166 | 1.885 → 1.667 |
| translate | 0.109 → 0.054 | 0.778 → 0.829 | 0.657 → 0.684 | 1.487 → 1.814 | 2.046 → 2.795 |
| rotate | 0.100 → 0.050 | 0.806 → 0.773 | 0.662 → 0.623 | 1.581 → 1.481 | 2.496 → 2.027 |
| seam_edits | 0.199 → 0.070 | 1.907 → 1.897 | 1.765 → 1.751 | 3.112 → 3.187 | 3.946 → 3.901 |
| sky_moving | 0.169 → 0.164 | 0.784 → 0.654 | 0.624 → 0.488 | 1.591 → 1.329 | 2.120 → 1.942 |
| wall_moving | 0.106 → 0.051 | 0.843 → 0.841 | 0.638 → 0.696 | 1.789 → 1.550 | 2.585 → 2.114 |
| cloud_layer | 0.161 → 0.149 | 0.646 → 0.583 | 0.504 → 0.450 | 1.291 → 1.200 | 1.952 → 1.825 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| underground | 0.815–0.984 | 0.843–1.002 | 0.104–0.111 | 0.050–0.069 |
| surface_still | 0.727–0.860 | 0.640–0.693 | 0.102–0.113 | 0.052–0.056 |
| overview | 0.703–0.874 | 0.671–0.802 | 0.084–0.088 | 0.001–0.001 |
| sky | 0.639–0.798 | 0.571–0.642 | 0.152–0.167 | 0.116–0.133 |
| translate | 0.749–0.922 | 0.804–1.023 | 0.104–0.115 | 0.053–0.055 |
| rotate | 0.774–0.955 | 0.764–0.784 | 0.099–0.104 | 0.049–0.052 |
| seam_edits | 1.709–2.219 | 1.858–1.999 | 0.137–0.230 | 0.067–0.104 |
| sky_moving | 0.653–0.887 | 0.611–0.719 | 0.132–0.215 | 0.121–0.190 |
| wall_moving | 0.763–0.853 | 0.760–0.847 | 0.104–0.119 | 0.050–0.052 |
| cloud_layer | 0.607–1.270 | 0.572–0.587 | 0.127–0.335 | 0.124–0.157 |

### 1920x1080 night

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| underground | 0.236 → 0.113 | 1.408 → 1.299 | 0.911 → 0.863 | 2.062 → 2.022 | 2.591 → 2.746 |
| surface_still | 0.241 → 0.125 | 1.676 → 1.484 | 0.772 → 0.708 | 1.909 → 1.590 | 2.953 → 2.367 |
| overview | 0.206 → 0.001 | 1.354 → 1.107 | 0.656 → 0.587 | 1.793 → 1.588 | 2.702 → 2.111 |
| sky | 0.280 → 0.280 | 1.008 → 0.992 | 0.497 → 0.514 | 1.340 → 1.293 | 2.127 → 2.063 |
| translate | 0.259 → 0.117 | 1.589 → 1.361 | 0.662 → 0.713 | 1.624 → 1.715 | 2.433 → 2.446 |
| rotate | 0.237 → 0.114 | 1.334 → 1.191 | 0.784 → 0.713 | 1.749 → 1.611 | 2.385 → 2.354 |
| seam_edits | 0.233 → 0.110 | 2.007 → 1.810 | 1.763 → 1.646 | 3.200 → 2.917 | 3.895 → 3.588 |
| sky_moving | 0.283 → 0.279 | 1.006 → 1.011 | 0.502 → 0.509 | 1.333 → 1.375 | 1.842 → 2.247 |
| wall_moving | 0.231 → 0.113 | 1.488 → 1.145 | 0.728 → 0.672 | 1.880 → 1.721 | 2.555 → 2.400 |
| cloud_layer | 0.289 → 0.286 | 0.890 → 0.835 | 0.488 → 0.480 | 1.425 → 1.290 | 2.180 → 1.955 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| underground | 1.406–1.500 | 1.268–1.486 | 0.231–0.236 | 0.112–0.127 |
| surface_still | 1.640–1.808 | 1.471–1.524 | 0.241–0.257 | 0.116–0.135 |
| overview | 1.327–1.499 | 1.095–1.114 | 0.203–0.207 | 0.001–0.001 |
| sky | 0.989–1.178 | 0.986–1.031 | 0.273–0.282 | 0.277–0.282 |
| translate | 1.480–1.680 | 1.344–1.369 | 0.238–0.275 | 0.116–0.127 |
| rotate | 1.328–1.440 | 1.188–1.218 | 0.235–0.237 | 0.114–0.117 |
| seam_edits | 1.814–2.159 | 1.787–1.822 | 0.224–0.291 | 0.109–0.116 |
| sky_moving | 1.000–1.108 | 0.985–1.124 | 0.278–0.284 | 0.279–0.284 |
| wall_moving | 1.306–1.541 | 1.122–1.174 | 0.230–0.256 | 0.113–0.116 |
| cloud_layer | 0.830–0.894 | 0.824–0.837 | 0.286–0.307 | 0.285–0.290 |

### 1920x1080 dawn surface

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| surface_still | 0.244 → 0.117 | 1.708 → 1.594 | 0.917 → 0.854 | 1.940 → 1.942 | 2.488 → 2.830 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| surface_still | 1.668–1.817 | 1.559–1.596 | 0.230–0.244 | 0.116–0.117 |

### 1920x1080 dusk surface with F3

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| surface_still | 0.237 → 0.119 | 1.792 → 1.670 | 1.209 → 1.237 | 2.630 → 2.788 | 3.482 → 3.890 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| surface_still | 1.778–1.863 | 1.667–1.690 | 0.235–0.237 | 0.119–0.124 |

### Focused 1280x720 translation follow-up

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| translate | 0.104 → 0.054 | 0.935 → 0.910 | 0.776 → 0.729 | 1.865 → 1.786 | 2.492 → 2.400 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| translate | 0.881–1.017 | 0.880–1.064 | 0.103–0.133 | 0.053–0.073 |

### Focused 1920x1080 wall-motion follow-up

| Scene | Cloud GPU mean | Frame mean | Frame median | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| wall_moving | 0.243 → 0.114 | 1.542 → 1.096 | 1.005 → 0.949 | 2.267 → 1.896 | 3.484 → 2.481 |

| Scene | Baseline frame-mean range | Candidate frame-mean range | Baseline cloud-GPU range | Candidate cloud-GPU range |
| --- | ---: | ---: | ---: | ---: |
| wall_moving | 1.310–2.871 | 1.066–2.332 | 0.223–0.461 | 0.110–0.223 |

### CPU submission, GPU completion, and edit work

For the 1080p day suite, medians of per-trial means are:

| Scene | Full CPU submission | Whole-frame GPU elapsed | Instrumented cloud CPU submission |
| --- | ---: | ---: | ---: |
| surface_still | 0.603 → 0.690 | 1.515 → 1.312 | 0.029 → 0.038 |
| overview | 0.714 → 0.601 | 1.295 → 1.047 | 0.060 → 0.001 |
| seam_edits | 1.778 → 1.770 | 1.560 → 1.418 | 0.056 → 0.053 |
| cloud_layer | 0.638 → 0.508 | 0.675 → 0.690 | 0.005 → 0.004 |

Partial views add viewport/scissor queries and scissor state changes. Their
instrumented cloud CPU submission can therefore rise: 0.029 → 0.038 ms in
the surface scene above, alongside 0.248 → 0.113 ms of cloud GPU time.
Empty views remove the old state/uniform submission entirely. Full CPU
submission also includes driver waits and varies with the rest of the frame.

Unedited scenes retain zero terrain uploads and rebuilds. Seam edits retain
two rebuilt chunks, four uploads, and 167,664 uploaded bytes per measured frame.
Their 1080p day CPU rebuild mean is 0.879 → 0.906 ms; the cloud change does not
alter meshing. Terrain draw/triangle counts and the 13,353,624-byte initial
geometry upload are unchanged. These are work/payload counts, not process-memory
or startup-throughput measurements.

Final batch drains remain included in all frame means. They sometimes exceed
a second, so a large final interval is not evidence of a displayed gameplay
hitch. The following counts separate nonfinal intervals over 16.667 ms while
retaining final-drain timing in the recorded totals. The
[raw-derived audit summary](cloud-performance-audit.csv) records the per-variant
counts, drain maxima, and initial geometry payload:

| Suite | Nonfinal >16.667 ms, before → after | Largest nonfinal interval, before → after | Largest final GPU drain, before → after |
| --- | ---: | ---: | ---: |
| 1920x1080 day | 2 → 0 | 27.706 → 5.555 | 471.706 → 385.864 |
| 1280x720 day | 0 → 0 | 6.499 → 9.054 | 216.631 → 28.760 |
| 1920x1080 night | 0 → 1 | 9.846 → 20.733 | 520.937 → 419.692 |
| 1920x1080 dawn surface | 0 → 0 | 4.178 → 9.248 | 406.647 → 354.938 |
| 1920x1080 dusk surface with F3 | 0 → 0 | 6.933 → 5.265 | 244.387 → 190.470 |
| Focused 1280x720 translation follow-up | 0 → 0 | 5.505 → 3.804 | 1.508 → 2.522 |
| Focused 1920x1080 wall-motion follow-up | 1 → 0 | 25.050 → 4.708 | 1050.950 → 706.426 |

The remaining full-screen cloud cost in skyward and inside-layer views is an
opportunity for a separate shader profile. This pass keeps the established
ray march intact; no quality-reducing alternative was retained.

## Reproduction

Prepare separate worktrees at the two revisions above. Run the original
baseline's `build.cmd -Test` before applying test overlays. Copy the candidate's
three benchmark files listed above into the baseline so both benchmark binaries
use identical fixtures. The new no-draw regression intentionally fails on old
production code, so build the baseline benchmark in profiling mode:

```powershell
$env:KERNELCRAFT_RENDER_PROFILE = '1'
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_PHASE = '0.125'
$env:KERNELCRAFT_PROFILE_SCENE = 'overview'
.\build.cmd -Benchmark
$env:KERNELCRAFT_RENDER_PROFILE = $null
$env:KERNELCRAFT_PROFILE_SCENE = $null
```

Build the candidate with `build.cmd -Benchmark`, then run from the candidate
checkout, using the actual baseline and candidate paths:

```powershell
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_PHASE = '0.125'
$env:KERNELCRAFT_PROFILE_WIDTH = '1920'
$env:KERNELCRAFT_PROFILE_HEIGHT = '1080'
$env:KERNELCRAFT_PROFILE_SCENE = $null
$env:KERNELCRAFT_PROFILE_DEBUG = $null
$env:KERNELCRAFT_PROFILE_SKIP_TEXT = $null
.\tests\compare_render_profile.ps1 -BaseDirectory $baseline `
  -CandidateDirectory $candidate -OutputDirectory $newOutput -Pairs 3 -Pipelined
```

Repeat with width/height 1280/720 for day, phase 0.625 at 1080p for night, and
`KERNELCRAFT_PROFILE_SCENE=surface_still` with phase 0 or 0.5 for dawn/dusk.
Set `KERNELCRAFT_PROFILE_DEBUG=1` for the dusk F3 check. Use a new output directory
each time, run trials separately from builds/tests, and restore temporary
environment settings afterward. Raw per-frame CSVs and driver/batch logs are
written beside each suite's `runs.csv`.

## Validation

The original baseline passed native Windows `build.cmd -Test` before changes.
The candidate passed all of the following with exit code 0:

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
.\build.cmd -Benchmark
C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe `
  -NoProfile -ExecutionPolicy Bypass -File tests\test_build.ps1
```

The application tests exercised the actual game loop with scripted input:
walking, crouching, running, jumping, flight, timed breaking, placement,
selection, F3/F4, chat/time commands, cloud-layer pixels, focus/capture changes,
minimization, framebuffer resizing, shutdown, F5 saving, and process restart.
They use disposable saves. No existing player world was opened or modified.
Expected upload/build-failure injections completed with their asserted results.
The Windows incremental-build regression passed all 22 build steps, including
the two expected compilation failures and recovery.

Linux GCC and Clang CI passed Release/Debug CPU tests, address/undefined-behavior
sanitizers, hidden Mesa application/rendering tests, and the GCC incremental
build regression for the code commit. See the [CI run](https://github.com/frankischilling/kernelcraft/actions/runs/34555005077).
Mesa validates correctness here; its performance is not compared with Intel's.

Hands-on interactive playtesting, displayed-frame pacing, and input-to-frame
latency were not measured. The hidden scripted tests and uncapped rendering
profiler do not establish those results. Startup time, process memory, and save
throughput were not optimization targets or separately measured acceptance
metrics in this change.
