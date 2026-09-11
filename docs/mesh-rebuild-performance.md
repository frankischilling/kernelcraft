# Chunk rebuild performance

Chunk meshes retain compact rectangle records from one greedy sweep, then emit
the exact-sized vertex and index buffers in the existing material order.
Surface bounds accumulate in integral block coordinates and convert once to
world coordinates. These changes reduce synchronous edit and startup work.

The production change is `0ac85a2dcde4ef76ff7d0ab821e67a167407099e`, compared
with main at `a1deaf43028c75e0a5a7a15520255514dfa83697`. The profiling-window
correction is `a9ae23d`. The changes are on `perf/mesh-rebuild` in the unmerged
[PR #65](https://github.com/frankischilling/kernelcraft/pull/65),
linked to [issue #64](https://github.com/frankischilling/kernelcraft/issues/64).
The earlier [PR #60 results](pr60-performance.md) are a separate historical
comparison; its caches remain in use.

## Method and scope

Native Windows 11 Pro 10.0.22621, Core i7-13700H, Intel UHD Graphics,
OpenGL 4.6 compatibility, driver 32.0.101.7077. The installed NVIDIA RTX 2000
Ada GPU was not the active renderer. Both worktrees use MinGW GCC 13.2.0,
C11 Release `-O2 -Wall -Wformat=2 -Wstrict-prototypes -Werror`, identical
dependencies and assets, and the same final profiling header.

Each rendering suite uses three alternating baseline/candidate pairs, seed 0,
120 warm-up and 600 measured frames per scene. All ten existing scenes run at
verified 1280x720 and 1920x1080 framebuffers, with day phase 0.125 or night
phase 0.625. A further 1080p daytime suite enables F3. The deterministic camera,
70-degree FOV, six-chunk render radius, atmosphere phase and cloud drift paths
match across revisions. Normal HUD, sky, lighting, clouds and selection are
enabled. The benchmark's ordinary closed-chat HUD is used.

VSync is disabled, matching the current game's explicit `glfwSwapInterval(0)`.
Pipelined frame time includes setup, submission, swaps, bookkeeping and the
final GPU drain. GPU timer results and CSV writes occur after batch completion.
CPU submission includes mesh updates and visibility, but excludes swaps and
the final drain. GPU elapsed time can include starvation while the CPU rebuilds;
it is not an isolated shader or utilization measurement.

These are hidden-window rendering measurements, not input-to-display latency,
physical presentation pacing, player-physics throughput or file-I/O benchmarks.
No normal-loop profiling waits, queries, threads or dependencies were added.

## Results

All timing tables use milliseconds and the median of per-trial metrics, not
pooled percentiles. Reduction is `100 * (before - after) / before`. The main
table includes every initial suite with the final profiler; repeats follow separately.
Each side of each three-pair scene has 1,800 measured frames.

| Seam edits, three pairs | Frame mean before/after | Median before/after | p95 before/after | p99 before/after | Rebuild before/after |
| --- | ---: | ---: | ---: | ---: | ---: |
| 720p day | 1.619 / 1.460 | 1.441 / 1.297 | 2.619 / 2.474 | 3.089 / 3.225 | 0.811 / 0.571 |
| 720p night | 1.648 / 1.356 | 1.508 / 1.220 | 2.676 / 2.233 | 3.043 / 3.150 | 0.808 / 0.575 |
| 1080p day | 1.927 / 1.782 | 1.697 / 1.636 | 2.991 / 2.877 | 3.969 / 3.668 | 0.889 / 0.680 |
| 1080p night | 1.781 / 1.776 | 1.596 / 1.588 | 2.871 / 2.902 | 3.494 / 3.715 | 0.859 / 0.675 |
| 1080p day, F3 | 2.093 / 2.172 | 1.932 / 1.890 | 3.280 / 4.108 | 4.067 / 5.463 | 0.869 / 0.670 |

The five-pair repeats contain 3,000 frames per side per scene. The 720p repeats
run all ten scenes; the 1080p repeats select only `seam_edits`, so their world
has not passed through the preceding camera scenarios. Their baseline and
candidate fixtures still match each other. Do not pool them with the full suite.

| Seam edits, five-pair repeat | Frame mean before/after | p95 before/after | p99 before/after | Rebuild before/after | Frame reduction | Paired reduction range |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 720p day | 1.794 / 1.629 | 2.833 / 2.572 | 3.222 / 3.296 | 0.916 / 0.640 | 9.2% | -0.1% to +25.6% |
| 720p night | 2.016 / 1.532 | 3.451 / 2.446 | 4.247 / 3.144 | 0.942 / 0.627 | 24.0% | +8.8% to +31.9% |
| 1080p day, F3 | 2.595 / 2.321 | 4.173 / 3.828 | 5.405 / 5.142 | 0.956 / 0.687 | 10.5% | -26.2% to +12.5% |
| 1080p night | 1.945 / 1.741 | 3.040 / 2.901 | 3.635 / 3.710 | 0.890 / 0.633 | 10.5% | +3.5% to +20.5% |

The consistent result is lower synchronous rebuild cost. Full-frame results
depend on GPU/driver and scheduling: the first 1080p night result was effectively
unchanged, and the first F3 result was slower. Repeats improved their means, but
p99 did not consistently improve. No general frame-pacing or input-latency gain
is established. All trials, including regressions, remain in the CSVs.

The 720p sky and moving-view slowdowns varied substantially on repetition.
For example, the daytime sky mean changed from 0.505/0.634 in the first final
suite to 0.676/0.652 in the repeat. Unedited scenes execute no meshing, and
their geometry and uploads match. These differences do not establish a change
in steady rendering cost. One candidate nighttime translation trial spent
610.884 ms in its final frame, which includes the deliberate final GPU drain;
its CPU submission for that frame was 0.480 ms. This is retained in throughput
and maximum-frame accounting, not described as an interactive gameplay hitch.

Complete scene coverage, three-pair frame means:

| Scene | 720p day before/after | 720p night before/after | 1080p day before/after | 1080p night before/after |
| --- | ---: | ---: | ---: | ---: |
| underground | 0.647 / 0.778 | 0.781 / 0.769 | 1.295 / 1.293 | 1.588 / 1.426 |
| surface_still | 0.761 / 0.758 | 0.775 / 0.755 | 1.490 / 1.561 | 1.657 / 1.613 |
| overview | 0.709 / 0.670 | 0.653 / 0.782 | 1.450 / 1.315 | 1.389 / 1.374 |
| sky | 0.505 / 0.634 | 0.563 / 0.656 | 0.767 / 0.783 | 0.999 / 1.044 |
| translate | 0.663 / 0.786 | 0.856 / 0.760 | 1.357 / 1.369 | 1.690 / 1.613 |
| rotate | 0.777 / 0.714 | 0.786 / 0.726 | 1.238 / 1.353 | 1.573 / 1.557 |
| seam_edits | 1.619 / 1.460 | 1.648 / 1.356 | 1.927 / 1.782 | 1.781 / 1.776 |
| sky_moving | 0.579 / 0.755 | 0.529 / 0.576 | 0.779 / 0.802 | 1.118 / 1.058 |
| wall_moving | 0.676 / 0.723 | 0.663 / 0.694 | 1.181 / 1.172 | 1.359 / 1.313 |
| cloud_layer | 0.514 / 0.515 | 0.533 / 0.573 | 0.769 / 0.716 | 0.832 / 0.828 |

CPU-only build/free results use five alternating pairs, 128 measured builds per
trial (640 per side per fixture). Fingerprints of vertices, indices, batches
and bounds match in all five fixtures.

| Chunk | Mean before/after | Median before/after | p95 before/after | p99 before/after | Mean reduction | Trial-mean range before / after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| generated | 0.2913 / 0.2086 | 0.2686 / 0.1686 | 0.6003 / 0.3628 | 0.6365 / 0.3852 | 28.4% | 0.2886 to 0.3917 / 0.1724 to 0.2294 |
| empty | 0.0102 / 0.0122 | 0.0101 / 0.0110 | 0.0113 / 0.0156 | 0.0114 / 0.0157 | -20.0% | 0.0089 to 0.0193 / 0.0116 to 0.0151 |
| sparse | 0.0474 / 0.0371 | 0.0454 / 0.0337 | 0.0597 / 0.0453 | 0.1437 / 0.1007 | 21.9% | 0.0384 to 0.0801 / 0.0311 to 0.0443 |
| solid | 0.2542 / 0.1754 | 0.2273 / 0.1642 | 0.4154 / 0.2488 | 0.4677 / 0.2965 | 31.0% | 0.2095 to 0.3618 / 0.1625 to 0.1813 |
| checkerboard | 2.9795 / 2.8530 | 2.8592 / 2.7085 | 4.1932 / 3.7643 | 4.9174 / 4.2717 | 4.2% | 2.8721 to 3.6190 / 2.7775 to 2.9187 |

Generated and solid chunks improve by 28.4% and 31.0%. The empty fixture is
about 0.002 ms slower in this sample; no empty-chunk gain is claimed. The
checkerboard mean changes by only 4.2% amid overlapping trial ranges, so its
speedup is inconclusive. The extra temporary allocation is an explicit cost.

Across the 15 trials per revision in the five primary suites, median warm renderer
initialization fell from 108.472 to 77.488 ms (28.6%). This interval includes
texture/grid setup, mesh construction, visibility/occluder data, uploads and
GPU completion; it is not pure meshing or complete application startup.
Generation measured 32.498/33.128 ms and its code/output are unchanged. No
cold-cache loading or save-I/O improvement is claimed.
Renderer initialization trial ranges were 89.909 to 149.647 ms before and
69.487 to 85.315 ms after; generation ranges were 29.527 to 41.891 ms before
and 29.287 to 36.190 ms after.

Every seam frame rebuilds two chunks, issues four terrain buffer uploads and
uploads 167,664 bytes. Unedited frames rebuild/upload nothing. Terrain draw,
triangle and surface-block counts match for every paired scene. These counters
cover terrain; they do not enumerate compatibility-mode HUD/selection commands.
The whole seeded world retains 210,404 exposed unit faces, 79,481 greedy quads
and 13,352,808 mesh bytes. Initial buffer uploads include the grid for a total
of 13,353,624 bytes. CPU block storage remains 4,194,304 bytes.

Requested live allocation peaks (separate instrumented runs, bytes):

| Chunk | Before | After | Added temporary request |
| --- | ---: | ---: | ---: |
| Generated | 77,616 | 84,105 | 6,489 |
| Empty | 0 | 0 | 0 |
| Sparse | 1,008 | 1,050 | 42 |
| Solid | 1,008 | 33,264 | 32,256 |
| Checkerboard | 8,257,536 | 8,601,600 | 344,064 |

All requested mesh allocations return to zero after each build/free sequence.
GCC `-fstack-usage` reports a 39,872-byte candidate build frame. The baseline
reports 38,640 bytes plus its 1,552-byte greedy helper frame. These reports
exclude library callees and driver memory. Process resident/peak memory,
isolated GPU upload completion cost and physical display latency are unmeasured.

The [trial directory](benchmarks/mesh-2026-09-11) contains 640 render scene
summaries (384,000 measured frames including exploratory runs), 82 startup
trials, CPU trials, allocation counts, capture hashes and environment details.
Rendering rows preserve mean/median/p95/p99, CPU and GPU means, maximum frame
and CPU times, frames over 5 ms, final-frame cost, and deterministic work counts.
Full per-frame CSVs and logs remain in the local comparison output archive.


## Ownership and compatibility

Each temporary record contains seven byte-sized fields: face, slice, row,
column, width, height and material. Its allocation is bounded by the number
of exposed unit faces. Every greedy rectangle consumes at least one such face.
With the current dimensions, the conservative bound is 688,128 temporary bytes;
the full checkerboard requests 344,064. Nonempty meshes make one additional
allocation, released before returning. Empty meshes allocate nothing. There
is no retained scratch cache, and the GPU mesh payload is unchanged.

Checked size/index bounds precede output allocation. Every allocation failure
releases partial storage and returns an empty result. The caller still owns
the resulting mesh buffers, and upload failures retain their existing handling.
Seam invalidation and immediate rebuilds are unchanged. Detached chunks,
invalid IDs, material ordering, repeated UVs, outward winding and bounds retain
their existing semantics. No GL version, shader, asset, save format, world
content, physics rule or visual-quality setting changes.

## Reproduction

Create separate baseline and candidate worktrees. Copy the candidate's
`tests/render_profile.h` and `tests/mesh_profile.c` into the baseline so the
measurement implementation is identical. Build both with `build.cmd -Benchmark`.
From the candidate, run the existing comparison helper with a new output
directory for every suite:

```powershell
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_PHASE = '0.125'
$env:KERNELCRAFT_PROFILE_WIDTH = '1920'
$env:KERNELCRAFT_PROFILE_HEIGHT = '1080'
./tests/compare_render_profile.ps1 -BaseDirectory <baseline> -CandidateDirectory <candidate> -OutputDirectory <new-directory> -Pairs 3 -Pipelined
```

Use phase `0.625` for night, width/height `1280`/`720` for the established
comparison size, and `KERNELCRAFT_PROFILE_DEBUG=1` for F3. Clear the F3, scene
and skip-text overrides for the ordinary complete suite. Restore temporary
environment settings afterward. The helper restores its own profile/CSV/pipeline
settings. The hidden profiling window omits decorations because Windows
otherwise clamps a 1920x1080 request to 1920x1055 on this desktop. It rejects
a framebuffer that does not match the requested size and prints both sizes.

Compile the same CPU fixture in each worktree. On native Windows with the
matching compiler directory on PATH:

```powershell
$worldSources = @(Get-ChildItem src/world/*.c | ForEach-Object FullName)
gcc -std=c11 -O2 -Wall -Werror -Isrc tests/mesh_profile.c $worldSources src/math/math.c src/graphics/frustum.c src/utils/raycast.c -lm -o mesh-profile.exe
./mesh-profile.exe
```

The fixture performs eight warm-ups and 128 timed build/free iterations per
scene, and hashes vertices, indices, batches and bounds outside timed samples.
For requested-allocation accounting, compile a separate diagnostic binary with
`-DKERNELCRAFT_PROFILE_ALLOCATIONS -Wl,--wrap=malloc -Wl,--wrap=free`.
Do not include that instrumented binary in timing comparisons. Its peak is
live requested mesh allocations, not process working set or driver memory.

## Validation and limits

All following checks passed on the final production code:

- Native Windows `./build.cmd -Test` in Release and
  `./build.cmd -Configuration Debug -Test`: CPU, startup/input, persistence,
  shader/texture, HUD, rendering and failure-path checks, exit 0. The expected
  injected upload, missing-asset and save errors are part of these fixtures.
- Native Windows `./build.cmd -Benchmark`, exit 0; all alternating render
  trials also passed their GL, draw-count, immediate-rebuild and idle-upload
  invariants. Paired output comparison confirmed matching work counters/assets.
- `tests/test_build.ps1` using Windows PowerShell: 22 build stages passed,
  exit 0. An initial invocation using the bare `powershell` name failed because
  it was absent from PATH; the absolute installed executable ran the suite.
- [Linux CI at the final code revision](https://github.com/frankischilling/kernelcraft/actions/runs/34551536416):
  GCC and Clang Release/Debug builds and CPU tests, address/undefined-behavior
  sanitizers, hidden Mesa application/rendering tests, and GCC incremental
  build regressions passed. These ran on GitHub Ubuntu 24.04, not locally on
  Windows. No Linux or software-renderer performance gain is claimed.
- Independent CPU surface coverage, material/winding/UV checks, seeded-world
  fingerprints, all five mesh fingerprints, detached chunks, seam edits,
  invalid IDs, repeated full checkerboards, and cleanup/retry after each of
  the three mesh allocation failures passed. The failure tests also passed
  against the baseline's two-allocation implementation before optimization.
- Nine controlled day/dawn/dusk/night/celestial/cloud captures and one
  selection capture are byte-identical. Four terrain/HUD captures differ
  only within the live update-time glyph at x=195..202, y=134..146
  (0.01 versus 0.00 ms); every pixel outside that region matches.
  [Capture hashes](benchmarks/mesh-2026-09-11/captures.json) retain those differences.
- Formatting, staged-diff checks, and independent code review passed.

The application fixtures exercise real game-loop rendering with scripted
walking, crouching/running/jumping, flight, collision, timed breaking,
placement, selection after edits, F3/F4, chat/time commands, capture/focus,
resizes/minimization, F5, save failure, restart and normal shutdown. They use
temporary explicit save paths. These are automated checks, not hands-on
interactive playtesting. Physical monitor scaling, visible presentation and
input-to-display latency were not tested.

The runtime review covered the loop, fixed-step inputs/physics, selection and
edits, dirty propagation, greedy meshing, visibility/occluders, uploads,
sky/cloud/terrain/HUD passes, generation and persistence. The initial complete
render diagnostic identified seam rebuilds as a substantial CPU cost. A separate
temporary mesh-stage diagnostic attributed about 80% of generated-chunk work
to the repeated greedy sweeps; solid-chunk exposure/bounds work was also costly.
An attempted MinGW gprof run produced no samples, so it supplied no attribution.

The initial decorated-window 720p trials and failed 1080p attempt are retained
separately from the final suites. No 1920x1055 result is reported as 1080p.
Descriptor-only and integral-bounds experiments preceded the final combined
patch; preliminary timings are exploratory, not final acceptance results.

Output-heavy checkerboards still require large vertex/index buffers. Further
work on vertex emission or upload cost would need new stage measurements.
The current finite-world dirty scan costs only a few microseconds per unedited
render frame, so this pass does not add a dirty queue or deferred updates.
