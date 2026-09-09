# Occlusion rendering benchmark

This report compares PR #49's production renderer at `1972ec2` with its base
`71c710b`. Both builds use the same additional profiling harness. No production
code was changed for this follow-up measurement.

These results predate the subsequent [sky-motion fix](sky-motion-culling.md),
which adds mesh-surface frustum checks. The current profiling harness also has
an eighth `sky_moving` scene; it was not part of the seven-scene runs below.

The repeated benchmark does **not establish an FPS improvement**. Geometry
submissions fall substantially in occluded stationary views, but median FPS
fell in this run set, especially during movement. The zero-terrain sky workload
also varies widely, limiting precise attribution. It still includes the PR's
visibility bookkeeping, so it is not a no-change control. These results replace
the earlier single-run timing claim;
this change should not be described as a demonstrated FPS gain.

## Setup

- September 9, 2026; native Windows 11 Pro 10.0.22621; Intel Core i7-13700H.
- OpenGL ran on Intel UHD Graphics, driver 32.0.101.7077, OpenGL 4.6. The
  machine also has an NVIDIA GPU, which was not used in these runs.
- Existing High performance power scheme; clocks and thermal state were not
  locked. Other applications were left as they were. No other build or graphics
  test was launched during the measurements.
- GCC 13.2.0, native Release, C11, `-O2`, repository warning flags and `-Werror`.
- Seed 0, 1280x720 hidden framebuffer, normal HUD and selection enabled, F3
  hidden, swap interval zero. Assets and scene inputs match between revisions.
- Five process pairs, reversing the baseline/candidate order every pair:
  AB, BA, AB, BA, AB. Each scene has 120 warm-up frames and 600 measured frames.
  This gives 3,000 measured frames per scene per revision, 42,000 total.

The [per-run CSV](benchmarks/occlusion-2026-09-09-runs.csv) preserves all 70
scene results. The [initialization CSV](benchmarks/occlusion-2026-09-09-init.csv)
preserves the ten process initialization measurements. Every summary was
independently recomputed from the per-frame records and checked against the
harness output. No measured runs were removed.

## What the numbers mean

Frame timing starts before the clear and includes `renderWorld`, selection,
the normal HUD, and `glFinish`. It excludes input, physics, view-matrix setup,
buffer presentation, and writing the CSV. FPS is `1000 / mean frame ms`.
It is a serialized **render-loop equivalent**, not measured interactive FPS:
`glFinish` prevents normal CPU/GPU frame overlap and makes previous occlusion
queries ready before the next frame.

CPU submission measures time before `glFinish` and can include driver stalls.
The GPU timer measures the elapsed OpenGL interval; it can include GPU idle
time waiting for CPU commands, especially during mesh rebuilds. These values
do not measure CPU or GPU utilization, and their sum need not equal frame time.

P95/P99 use nearest-rank frame times. The 1% low is the reciprocal of the mean
of the slowest six frames in each 600-frame run. Reported summary cells are
medians of the five run-level measurements, not percentiles of pooled samples.

The scenes are:

- `underground`: position (0, 10, 3), yaw 90, pitch -10; an occlusion stress view.
- `surface_still`: the seed-0 spawn eye at (0.5, 13.62, 3.5), yaw 90, pitch -10.
- `overview`: position (0, 32, 3), yaw 90, pitch -45.
- `sky`: the spawn X/Z at height 40, yaw 90, pitch 89; no terrain submissions.
- `translate`: the spawn view translates 0.015 blocks in X per frame for 600
  frames. This is a fixed-height camera path, not a physics simulation.
- `rotate`: the spawn view rotates 0.3 degrees of yaw per frame.
- `seam_edits`: a block at (-1, 12, 6) toggles between air and a solid material.
  Every frame must rebuild exactly two neighboring chunks.

Terrain draws and triangles exclude the grid and immediate-mode HUD/selection.
Upload bytes count `glBufferData` payloads, excluding image transfers and driver
allocation overhead. Query counts include only terrain sample queries, not
the benchmark's GPU timer.

## FPS and variability

These are median run-level results. Change is the ratio of the two median FPS
values. The paired range shows the smallest and largest FPS change among the
five corresponding pairs; it is not a confidence interval.

| Scene | Base FPS | PR FPS | Change | Paired change range | 1% low base / PR |
| --- | ---: | ---: | ---: | ---: | ---: |
| underground | 243.9 | 202.5 | -17.0% | -28.9% to +117.7% | 86.6 / 76.7 |
| surface_still | 185.1 | 176.8 | -4.5% | -18.9% to +256.8% | 68.8 / 74.6 |
| overview | 192.5 | 191.4 | -0.6% | -43.6% to +33.3% | 76.7 / 75.5 |
| sky | 268.3 | 229.4 | -14.5% | -34.9% to +87.1% | 110.0 / 96.2 |
| translate | 213.4 | 152.7 | -28.4% | -47.8% to +18.6% | 73.6 / 64.2 |
| rotate | 190.0 | 128.2 | -32.5% | -56.7% to +16.3% | 76.2 / 45.4 |
| seam_edits | 144.9 | 131.5 | -9.2% | -30.6% to +7.0% | 74.3 / 45.5 |

The sky workload fell 14.5% even though both revisions submit zero terrain
triangles. The PR still reads visibility state, traverses chunk records, and
formats additional HUD statistics, so some decline may be PR overhead. Paired
sky changes range from -34.9% to +87.1%. Thermal/clock state, driver scheduling,
and unrelated system activity were not isolated. This variation limits the
precision of any claimed performance difference; it does not erase the observed
regressions. Moving views draw and query every candidate without geometry savings.

## Frame and submission time

Milliseconds, base / PR. CPU is CPU/driver submission; GPU is elapsed interval,
not utilization or pure GPU busy time.

| Scene | Frame P50 | Frame P95 | Frame P99 | CPU mean | GPU mean |
| --- | ---: | ---: | ---: | ---: | ---: |
| underground | 3.608 / 4.397 | 7.173 / 9.093 | 10.305 / 11.851 | 2.089 / 2.647 | 1.770 / 1.788 |
| surface_still | 4.928 / 4.821 | 9.344 / 10.802 | 12.845 / 12.565 | 2.662 / 2.591 | 2.156 / 2.678 |
| overview | 4.436 / 4.742 | 9.474 / 9.753 | 11.267 / 11.796 | 2.273 / 2.525 | 2.242 / 2.161 |
| sky | 3.269 / 4.065 | 6.185 / 6.966 | 8.078 / 8.762 | 2.190 / 2.356 | 1.008 / 1.174 |
| translate | 3.917 / 6.231 | 8.105 / 10.947 | 10.962 / 13.750 | 2.380 / 2.809 | 1.482 / 2.411 |
| rotate | 4.643 / 7.273 | 9.823 / 12.289 | 12.031 / 16.453 | 2.405 / 3.193 | 2.146 / 2.994 |
| seam_edits | 6.512 / 7.057 | 10.826 / 13.643 | 12.711 / 19.066 | 3.680 / 4.449 | 2.808 / 2.976 |

## Submitted work

Average per measured frame. These counts repeat across all five trials.

| Scene | Terrain draws base / PR | Triangles base / PR | PR sample queries |
| --- | ---: | ---: | ---: |
| underground | 42.00 / 25.00 | 22,890.00 / 16,256.00 | 0.00 |
| surface_still | 42.00 / 11.00 | 22,890.00 / 8,466.00 | 0.00 |
| overview | 48.00 / 45.00 | 25,464.00 / 22,684.00 | 0.00 |
| sky | 0.00 / 0.00 | 0.00 / 0.00 | 0.00 |
| translate | 41.30 / 41.30 | 23,685.33 / 23,685.33 | 41.30 |
| rotate | 42.55 / 42.55 | 24,020.25 / 24,020.25 | 42.55 |
| seam_edits | 42.00 / 42.00 | 22,896.00 / 22,896.00 | 42.00 |

At spawn, this removes **73.8% of terrain draws and 63.0% of submitted
triangles**. The overview removes only 6.3% of draws and 10.9% of triangles.
Culling does not reduce stored mesh size or the cost of rebuilding an edited
chunk. The original pixel regressions still verify that hidden geometry can
be omitted without changing the image.

## Edits, uploads, and initialization

Steady scenes performed zero buffer uploads and rebuilt zero chunks. Each seam
edit rebuilt exactly two chunks, using four buffer uploads totaling 167,664
bytes (163.7 KiB) on both revisions. Median mean rebuild/submission time was
1.372 ms on the base and 1.549 ms on the PR. This
CPU/driver wall time is subject to the same run-to-run variation as FPS.

| Initialization/storage metric | Base | PR |
| --- | ---: | ---: |
| First world + renderer initialization, ms | 202.458 | 260.805 |
| Warm CPU world generation, ms | 33.227 | 32.142 |
| Warm renderer initialization + finish, ms | 178.548 | 190.910 |
| Initial geometry buffer payload, bytes | 13,353,624 | 13,353,624 |
| CPU block payload, bytes | 4,194,304 | 4,194,304 |
| Persistent terrain query objects | 0 | 256 |

First initialization is CPU/driver wall time without an explicit GPU completion
wait. It excludes window/context creation, shader loading, and HUD startup. Warm
renderer initialization includes mesh generation, image loading, GPU buffer
uploads, and completion wait. The 13,353,624-byte geometry payload includes
terrain and the grid; it is not total VRAM. The PR adds 256 query objects; their
driver allocation size was not measured. Texture memory and process RSS were
not measured.

## Reproduce

For the exact historical seven-scene workload, use the profiling header and
comparison driver from `0a72efa`. The current versions add a moving-sky scene.

Use a clean archive of `71c710b` for the baseline and this branch for the
candidate. Copy `tests/render_profile.h` to the baseline. In its
`tests/render_benchmark.c`, add `#include "render_profile.h"` beside the other
fixture headers, then copy this branch's `if (getenv("KERNELCRAFT_RENDER_PROFILE"))`
entry/cleanup block immediately after `__glewGetUniformLocation = countLookup;`.
Do not copy the rest of the newer benchmark or change production sources.
The exact same header must be compiled into both builds.

Build each native Release benchmark with the profiling environment enabled:

```powershell
$env:KERNELCRAFT_RENDER_PROFILE = "1"
.\build.cmd -Benchmark
Remove-Item Env:\KERNELCRAFT_RENDER_PROFILE
```

Then run the paired driver from the candidate checkout, choosing a new output
directory. It checks header hashes, runs sequentially, restores its environment
variables, and preserves raw frame CSVs, logs, environment metadata, and summaries.

```powershell
.\tests\compare_render_profile.ps1 `
  -BaseDirectory C:\path\to\baseline `
  -CandidateDirectory C:\path\to\candidate `
  -OutputDirectory C:\path\to\new-results `
  -Pairs 5
```

For one Linux profiling check, use
`KERNELCRAFT_RENDER_PROFILE=1 make benchmark`. Keep Windows and Linux timing
results separate. The normal benchmark behavior is unchanged when the variable
is absent. All profiles run disposable seed-0 worlds and never open save files.

A useful follow-up is reducing query overhead during movement and then testing
on a controlled GPU/power configuration with normal frame overlap. These
results support reduced geometry submissions, not a claim of higher gameplay FPS.
