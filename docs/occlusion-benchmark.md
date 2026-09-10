# Current-view occlusion measurements

The moving-wall scene submitted 35.7% fewer terrain draws and 38.9% fewer
triangles. Moving sky views submitted no terrain, down from two chunks on main.
These reductions were identical across all ten before/after pairs. Timing was
mixed: the median paired throughput change behind the wall was +0.8%, while
translation through open terrain was -7.2%. The measurements do not establish a
general FPS improvement.

## Revisions and environment

The baseline is main at `5097fb929ee64989a7248f61350eec4ef19b3287`. Its production
source was unchanged; the benchmark dispatch and the same profiling header were
added for measurement. The candidate production implementation is committed in
`3b8f545e277a7810e7a5d05d42d93979d05f07d1`. Measurements preceded that commit;
subsequent edits added regression coverage and the historical-build guard,
without changing the measured production behavior or profiling header.

Both builds used native Windows Release, GCC 13.2.0, `-O2`, seed 0,
1280 by 720 pixels, and swap interval 0. The machine was an i7-13700H running
Windows 11 Pro 10.0.22621. OpenGL reported Intel UHD Graphics, driver
32.0.101.7077. The installed NVIDIA GPU was not the renderer used for these runs.

Two series each ran five pairs, alternating baseline/candidate order within
each series. Each process measured nine scenes, with 120 warm-up frames and 600
measured frames per scene: 20 processes and 108,000 measured frames altogether.
Series A began at 00:49 UTC and series B at 01:10 UTC on September 10, 2026
(September 9 locally). All completed pairs from both series are retained.

Builds and other graphical tests were stopped during measurement. Background
applications, thermals, and clock speeds were not controlled. The wide paired
ranges below are a reason to repeat measurements on a quieter machine before
making a release performance claim.

## Timing method

The hidden GLFW harness runs the actual terrain renderer, normal HUD, selection,
and buffer swaps. F3 diagnostics are off. It follows fixed camera paths; it does
not run the interactive input and physics loop or measure visible presentation.
These are render-harness throughput results, not hands-on gameplay FPS.

Pipelined mode includes contiguous frame intervals, setup, bookkeeping, swaps,
and a final GPU drain in the completed batch time. GPU timer results and CSV
writes are deferred until the batch finishes. This avoids counting only how
quickly the CPU enqueues unfinished frames. The optional serialized mode waits
every frame and is diagnostic throughput; it was not used in these tables.

CPU submission time includes visibility, dirty-mesh work, GL submission, HUD,
and selection, but excludes swapping and the final drain. `GL_TIME_ELAPSED`
can include GPU queue starvation while the CPU computes visibility; it is not
GPU utilization or an isolated shader cost. The profiler's timer queries are
separate from its counted occlusion queries. Production creates no queries.

Frame times below are medians of the ten per-run means for each revision.
Paired throughput change is computed as
`100 * (baseline_frame_mean / candidate_frame_mean - 1)` within each pair,
then summarized by its median and full range. It is not a ratio of the two
independently selected medians. Positive values indicate higher throughput.

| Scene | Baseline ms | Candidate ms | Paired throughput median | Paired range |
| --- | ---: | ---: | ---: | ---: |
| Underground | 2.165 | 2.131 | +4.9% | -62.9% to +28.1% |
| Surface, still | 2.209 | 1.809 | +4.4% | -75.5% to +124.4% |
| Overview | 1.905 | 2.183 | -2.1% | -32.9% to +65.0% |
| Sky, still | 1.778 | 1.939 | -3.8% | -35.7% to +45.7% |
| Translate | 1.785 | 1.889 | -7.2% | -32.2% to +14.6% |
| Rotate | 2.008 | 1.977 | -3.8% | -48.2% to +40.2% |
| Seam edits | 3.046 | 3.195 | -5.7% | -66.6% to +14.8% |
| Sky, moving | 1.822 | 1.812 | +2.2% | -16.1% to +38.1% |
| Wall, moving | 1.978 | 1.862 | +0.8% | -18.5% to +69.4% |

The favorable still-camera numbers cannot be attributed to occlusion alone:
their submitted geometry is unchanged. The implementation also caches an
unchanged draw list and orders retained chunks near to far. There is no isolated
timing experiment separating these effects from run variability.

## Geometry and CPU cost

Draws and triangles are per-frame means over the identical 600-frame paths.
CPU times are medians of per-run submission means.

| Scene | Draws, base / candidate | Triangles, base / candidate | CPU ms, base / candidate |
| --- | ---: | ---: | ---: |
| Underground | 42 / 42 | 22,890 / 22,890 | 1.897 / 1.868 |
| Surface, still | 42 / 42 | 22,890 / 22,890 | 1.851 / 1.629 |
| Overview | 48 / 48 | 25,464 / 25,464 | 1.722 / 1.743 |
| Sky, still | 0 / 0 | 0 / 0 | 1.598 / 1.740 |
| Translate | 41.30 / 41.23 | 23,685.33 / 23,682.75 | 1.623 / 1.709 |
| Rotate | 42.55 / 41.50 | 24,020.25 / 23,165.94 | 1.778 / 1.805 |
| Seam edits | 42 / 42 | 22,896 / 22,896 | 2.851 / 2.999 |
| Sky, moving | 2 / 0 | 1,990 / 0 | 1.654 / 1.636 |
| Wall, moving | 41.73 / 26.82 | 24,850.07 / 15,183.42 | 1.763 / 1.694 |

The surface scene uses the seed-0 spawn eye `(0.5, 13.62, 3.5)`. Translation
advances X by 0.015 blocks per frame; rotation advances yaw by 0.3 degrees per
frame. Moving sky uses pitch 89 degrees at spawn height with a repeating small
position/yaw change. The wall scene adds opaque blocks at X 0 through 15,
Y 1 through 31, Z 8, then moves the camera sinusoidally behind them at Y 20.
The [harness](../tests/render_profile.h) defines every pose and edit.

Seam edits rebuild exactly two chunks and issue four uploads totaling 167,664
bytes per frame on both revisions. Median rebuild/submission time rose from
1.067 to 1.175 ms, about 10.1%. The added surface metadata has a real construction
cost. Idle scenes perform no geometry uploads or rebuilds. Neither revision
issued an occlusion query.

Median initial generation was 29.970 / 29.640 ms, and initial mesh construction
plus upload was 153.345 / 149.502 ms. The latter ranged from 134.878 to 215.482 ms
on the baseline and 134.344 to 351.059 ms on the candidate, so its lower candidate
median is not evidence of cheaper initialization. Initial upload volume was
13,353,624 bytes on both, including the 816-byte grid. Terrain vertex/index
payload remained 13,352,808 bytes.

The candidate retains 1,907,544 bytes of rectangle bounds for seed 0, plus owner
records. Fixed occluder arrays occupy 107,520 bytes and the software depth buffer
and transform occupy 36,944 bytes on this 64-bit build. These are CPU payloads,
not total process memory. World block storage remains 4,194,304 bytes.

The draw-list cache removes repeated visibility work while still. Frustum
refinement fixes the demonstrated moving-sky submissions. Whole-chunk occlusion
helps the constructed wall scene but leaves almost all geometry in open-terrain
translation. Further work should reduce visibility cost in those views and
repeat timing on other GPUs before expanding the occluder budget.

## Reproduction and raw results

Create clean checkouts of the baseline and candidate revisions above. Copy the
candidate's `tests/render_profile.h` into the baseline's `tests` directory and
apply [baseline-profile.patch](benchmarks/occlusion-2026-09-09/baseline-profile.patch)
there. This patch only adds profiling dispatch for the modern baseline API; it
does not use the older `KERNELCRAFT_BASELINE` compile mode. Build each checkout
with `./build.cmd -Benchmark`, then run from the candidate checkout:

```powershell
.\tests\compare_render_profile.ps1 `
  -BaseDirectory C:\work\kernelcraft-base `
  -CandidateDirectory C:\work\kernelcraft-candidate `
  -OutputDirectory C:\work\occlusion-measurement-1 `
  -Pairs 5 -Pipelined
```

Repeat with a new output directory for a second series. The script checks that
both profiling headers match, captures hardware details, and rejects incomplete
runs or failed draw/upload/rebuild invariants. Clear optional benchmark
environment overrides before building. The profiling header SHA-256 for both
recorded series is
`BA81474289D4B33945A70CEDA9E541348C214F37E7750E588DBFD1F395927D31`.

- [All 180 scene/run summaries](benchmarks/occlusion-2026-09-09/runs.csv), with a
  series column to preserve the two independent pair sequences.
- [Derived scene statistics](benchmarks/occlusion-2026-09-09/summary.csv), including
  p99 frame time, CPU time, GPU elapsed time, rebuild cost, and upload volume.
- [Every frame and original process log](benchmarks/occlusion-2026-09-09/frames-and-logs.zip),
  including both original environment files and per-series summaries.
- [Series A environment](benchmarks/occlusion-2026-09-09/environment-series-a.json)
  and [series B environment](benchmarks/occlusion-2026-09-09/environment-series-b.json).
- [Revision, source, and artifact hashes](benchmarks/occlusion-2026-09-09/manifest.json).
  Source hashes describe the local files committed in the candidate revision;
  line-ending conversion in a fresh checkout can change their byte hashes.

## Correctness and build validation

All commands below completed with exit status 0. Native Release was rerun after
the final benchmark-dispatch guard change; the other full suites passed before
that test-only compatibility fix.

| Environment | Commands |
| --- | --- |
| Native Windows / GCC | `.\build.cmd -Test`; `.\build.cmd -Configuration Debug -Test`; `.\tests\test_build.ps1` |
| WSL Ubuntu / GCC | `make -j4 all test`; `make test-sanitize`; `make test-gl`; `make test-build` |
| WSL Ubuntu / Clang | `make CC=clang CFLAGS='-O2 -g -Werror' all test`; `make CC=clang CFLAGS='-O2 -g -Werror' test-gl` |

Formatting and whitespace checks passed. The documented historical comparison
also compiled successfully against `12de8dd` with `KERNELCRAFT_BASELINE` after
guarding the modern profiling and occlusion-test dispatch.

The [correctness fixtures](occlusion-culling.md#checks) compare exact color and
depth against the real renderer with new visibility decisions bypassed. They
cover motion from the first frame, gaps, edits, near-plane crossings, projection
and viewport changes, wireframe, and 192 generated-terrain poses. Cache tests
change each input without a forced mesh rebuild, then check reuse on the next
unchanged frame. CPU sanitizers and hidden application, input, HUD, edit, and
save/restart checks passed. Tests used disposable worlds.

No hands-on interactive playtest or second-GPU performance run was performed.
