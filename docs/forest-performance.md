# Oak forests and chunk rendering

The default horizontal chunk-center radius is now 12 chunks (192 blocks), up
from 6 (96 blocks). `--render-distance N` accepts 1–16 chunks. The finite world
still contains 256 resident chunks; this change does not add streaming or LOD.
The [oak forest increment](oak-forests.md) separately adds trees and leafy ground.

## Changes retained

Visibility recomputation scans the clipped square neighborhood around the
camera, then applies the existing circular center-distance, frustum, and mesh
visibility tests. Unchanged views reuse their existing candidate list. Setting
the same distance or rejecting an invalid distance preserves that cache; a
successful distance change invalidates it. Stable insertion sorting is retained
after comparison with a fixed-buffer merge-sort alternative.

Chunk indices are packed to 16 bits for GPU upload when their maximum index is
at most 65,535. Larger meshes retain 32-bit indices. CPU meshes remain 32-bit;
visibility and occluder construction finish before the upload buffer is packed
in place. Terrain and shadow draws use the stored element type. Packing needs
no extra heap allocation. The shadow geometry list is cached until a mesh is
rebuilt, instead of scanning all chunks every unchanged frame.

For the unchanged generator-1 world, initial geometry uploads fall from
13,353,624 to 12,399,852 bytes, including the unchanged 816-byte grid. Terrain
index storage falls from 1,907,544 to 953,772 bytes. This is **50% less index
data and about 7.14% less total geometry upload data**, with unchanged vertices,
triangles, face materials, and UVs. It is not a measurement of total process
memory or total VRAM; textures, icons, render targets, and driver overhead are
outside these counters.

At radius 6, the rotating-camera scenario scans 169 chunk slots instead of the
previous full 256-slot search. Unchanged views scan zero slots after warm-up.
At the new radius 12, a camera near the center of this finite world still scans
all 256 slots when visibility changes. The neighborhood optimization mainly
helps smaller radii and positions near the world edges.

The generator-2 world has more geometry. Its initial vertex/index/grid payload
is 15,025,488 bytes at either render distance because all chunks are resident.
The CPU block array remains 4,194,304 bytes. Increasing the draw radius does not
allocate another copy of the world.

## Measurement method

The baseline is merged main `b0ffc6a`, preserved as a separate Release executable
and asset directory before edits. Candidate code is commit `0cd9384`, with the
merge-sort alternative recorded as a separate patch. Measurements ran on native Windows using
MSYS2 GCC 13.2.0 and Intel UHD Graphics, OpenGL 4.6 driver 32.0.101.7077.
The hidden-window profiler uses 1280×720, a 70-degree field of view, seed 0,
normal atmosphere/HUD rendering, 120 warm-up frames and 600 measured frames
per scene, pipelined GPU timers, and disabled swap interval.

There are three trials per variant across all ten existing scenarios. Variant
order rotates between trials. The matched group compares baseline generator 1
at radius 6 with the current renderer using either insertion or merge sort.
The forest group compares current generator 2 at radii 6 and 12, and repeats
radius 12 with merge sort. No build or other test suite from this task ran during profiling.

The legacy profiler stages an explicit generator-1 snapshot before meshing.
Its `generation_ms` includes staging/copying and is therefore **not comparable**
with the old executable's generation timing. Mesh upload and measured frames
use the same terrain. Per-frame draw counts, triangles, submitted surface
blocks, and rebuilt-chunk counts match exactly across all matched legacy runs;
the two forest radius-12 sort variants also match. Their signatures are in
[runs.csv](benchmarks/forest-2026-09-16/runs.csv).

[Raw frames and logs](benchmarks/forest-2026-09-16/frames-and-logs.zip),
[setup measurements](benchmarks/forest-2026-09-16/setup.csv), and
[environment and source hashes](benchmarks/forest-2026-09-16/environment.json)
record the complete trials. These are rendering measurements on one Windows
host, not displayed gameplay FPS or input-to-display latency.

## Same terrain and distance

Values below are medians of the three trial statistics. CPU is mean CPU
submission time per trial; frame and GPU are mean frame/GPU times. All times
are milliseconds. Full per-trial median, p95, and p99 data remain in the CSV.

| Scenario | Before frame | After frame | Before CPU | After CPU | Before p99 | After p99 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Surface still | 0.6051 | 0.6013 | 0.1150 | 0.1158 | 0.8442 | 0.8672 |
| Translate | 0.5655 | 0.5641 | 0.1476 | 0.1483 | 0.9878 | 0.9374 |
| Rotate | 0.5156 | 0.5178 | 0.1708 | 0.1754 | 1.0091 | 0.9456 |
| Seam edits | 2.6740 | 2.6100 | 1.6524 | 1.5462 | 3.9337 | 3.2849 |
| Moving wall | 0.4168 | 0.4258 | 0.1597 | 0.1559 | 0.8647 | 0.8607 |

The seam-edit scenario uploads 167,664 bytes per measured frame before and
155,688 afterward. Other matched scenarios upload no terrain bytes during
their measured frames. Frame and CPU timing changes are mixed outside the
edit scenario; no general frame-rate or frame-pacing improvement is claimed.

## Cost of the larger forest view

Both columns below use the current generator-2 forest and insertion sorting.
Only the radius changes. These comparisons measure additional visible work,
not an optimization against the older treeless world.

| Scenario | Radius 6 frame | Radius 12 frame | Radius 6 CPU | Radius 12 CPU | Radius 6 draws | Radius 12 draws |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Surface still | 0.5982 | 0.6706 | 0.1140 | 0.1499 | 42.00 | 90.00 |
| Translate | 0.5675 | 0.6142 | 0.1543 | 0.2449 | 41.23 | 88.24 |
| Rotate | 0.5268 | 0.5800 | 0.1722 | 0.2741 | 41.50 | 85.58 |
| Seam edits | 2.7831 | 2.8815 | 1.6108 | 1.7694 | 42.00 | 90.00 |

For example, the rotating view submits about 26,774 triangles per frame at
radius 6 and 63,621 at radius 12. Its mean frame time rises about 10% in these
trials. Other views and GPUs can behave differently; use a smaller configured
radius when the larger view is too costly.

## Sorting comparison

The alternative used stable bottom-up merge sorting with a 256-entry scratch
array. At forest radius 12, median CPU submission was 0.2401 ms for translation,
0.2831 ms for rotation, and 1.7157 ms for seam edits, compared with insertion's
0.2449, 0.2741, and 1.7694 ms. At legacy radius 6, the ordering also varies by
scene. These whole-renderer timings show no consistent sort winner, and no
isolated sort-time claim is made. The final implementation keeps stable
insertion sorting and avoids the extra 4,096-byte candidate scratch array.
The measured alternative is preserved as
[merge-sort.patch](benchmarks/forest-2026-09-16/merge-sort.patch).

## Reproduction and correctness checks

Build the native Release benchmark with `.\build.cmd -Test`. From its output
directory, select a matched world explicitly before running `benchmark.exe`:

```powershell
$env:KERNELCRAFT_RENDER_PROFILE = '1'
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_PIPELINED = '1'
$env:KERNELCRAFT_PROFILE_GENERATOR = '1'
$env:KERNELCRAFT_PROFILE_DISTANCE = '6'
$env:KERNELCRAFT_PROFILE_CSV = 'frames.csv'
.\benchmark.exe
```

Use generator `2` with distances `6` and `12` for the forest comparison. The
original baseline executable has fixed generator 1/radius 6 and ignores the
two new environment settings. Omit `KERNELCRAFT_PROFILE_SCENE` to run all ten
scenes. Apply the recorded patch and rebuild to reproduce the merge-sort
alternative. Rebuild without that patch for the final implementation.

Regressions cover radius bounds, unchanged/same/invalid setting cache reuse,
distance-change invalidation, signed world edges, visible geometry 112 blocks
away when changing from radius 6 to 12, and byte-identical framebuffer output
after compact and wide-index rebuilds. An isolated cube retains/uploads exactly
72 index bytes. A full parity checkerboard has 49,152 quads and correctly uses
1,179,648 bytes of 32-bit indices; both cases also exercise shadow redraws.
Tree material faces are compared with source PNGs and independent layer
oracles. The wireframe application fixture uses a temporary two-chunk radius
to isolate its wall while retaining its exact geometry-equality assertions,
then restores the normal distance before subsequent gameplay frames.

The native Release suite passes for both sorting variants, and the final
insertion-sort implementation passes the full native Debug suite. Linux CI
results are recorded in the pull request. Local `wsl --list --quiet`
did not return, so Linux checks use GitHub Actions rather than this host's WSL.
No hands-on keyboard/mouse playtest or second-GPU performance run is claimed.
