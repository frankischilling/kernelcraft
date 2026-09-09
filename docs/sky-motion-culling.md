# Sky-motion culling fix

At the seed-0 spawn eye `(0.5, 13.62, 3.5)`, pitch 89 degrees and yaw 90
degrees, a stationary sky view settled to zero terrain draws. A 0.05-degree
mouse rotation submitted two terrain draws and two queries again, although
they produced no visible depth pixels. Moving the camera had the same effect.

The chunk bounding boxes contain empty space above uneven terrain. Those
boxes can intersect the sky view after camera movement invalidates the old
occlusion result. The renderer now checks a hierarchy of actual greedy-mesh
rectangles against the current frustum before submitting terrain or queries.
This also runs during movement and wireframe rendering. It keeps the existing
rule that old GPU occlusion answers cannot hide newly exposed geometry.

## Regression results

The new production-renderer test first failed on the old implementation with
two draws, two queries, and zero visible pixels on every moving sky frame. It
now passes with zero draws and queries on all twelve frames. Each frame moves
the eye 0.0025 blocks and rotates yaw 0.05 degrees.

An overhead block appears on the first frame after placement: one draw and
7,759 depth pixels in the native 960x540 test. Removing it returns to zero
draws and queries immediately. Looking down then submits terrain again.
The existing wall, aperture, near-plane, inside-block, delayed-query,
wireframe, material, and selection regressions pass.

A separate diagnostic swept 192 poses: two eye heights, four pitches, and
24 yaw angles, followed by a 0.05-degree mouse rotation. Of the 131 sky-only
views, 35 resubmitted terrain after movement before the fix; none did afterward.
The other 61 views still contained visible geometry. This diagnostic used
the production renderer and a disposable seed-0 world.

CPU tests exercise empty meshes, separated surfaces whose combined bounds
overlap the view, all rectangle orientations, negative coordinates,
near/far-plane clipping, boundary contact, and a rotated frustum corner.

## Cost and benchmark method

Each nonempty mesh retains a CPU tree with one 32-byte leaf per quad and one
32-byte node per pair of children. For seed 0's 79,481 quads across 256 chunks,
that is 5,078,592 bytes (4.84 MiB) of node payload, excluding allocation metadata
and the small per-chunk owner records. This is calculated storage, not measured
process RSS. GPU mesh buffers and query-object counts are unchanged by this fix.
Tree construction runs at startup and on dirty mesh uploads; traversal makes
no per-frame allocations.

The follow-up compares production code at `0a72efa` (before this fix) with
`488531e` (after). Both builds use the identical updated profiling header,
including the eight-scene workload. The original seven scenes are unchanged;
`sky_moving` repeats the twelve-frame regression path at spawn height.

The measurement setup remains native Windows Release, GCC 13.2.0, Intel UHD
Graphics, seed 0, 1280x720, normal HUD, swap interval zero, 120 warm-up and
600 measured frames per scene. Five alternating pairs give 48,000 measured
frames. No other build or graphics test runs during measurement. Clocks and
thermal state are uncontrolled, and other applications remain open.

Frame times include `glFinish`, so the reported FPS is a serialized render-loop
equivalent, not interactive gameplay FPS. CPU submission can include driver
stalls, and GPU elapsed intervals can include waiting for CPU commands. Use
the [earlier report](occlusion-benchmark.md) for the full metric definitions.

## Repeated measurements

The [per-run CSV](benchmarks/sky-motion-2026-09-09-runs.csv) contains all 80
scene results; the [initialization CSV](benchmarks/sky-motion-2026-09-09-init.csv)
contains all ten initialization records. Every summary metric was checked
against the 48,000 raw frame records, with a separate review reproducing the
calculations. No measured runs were discarded.

Cells below are medians of five run-level values. Paired ranges are the smallest
and largest FPS changes within corresponding before/after pairs, not confidence
intervals. CPU and GPU columns are milliseconds.

| Scene | FPS before / after | Paired FPS change range | CPU ms before / after | GPU ms before / after |
| --- | ---: | ---: | ---: | ---: |
| underground | 209.7 / 226.0 | -54.7% to +33.6% | 2.325 / 2.606 | 1.300 / 1.327 |
| surface_still | 189.4 / 146.0 | -54.9% to +44.1% | 2.247 / 2.561 | 2.108 / 3.201 |
| overview | 126.4 / 180.4 | -6.1% to +53.2% | 2.498 / 2.748 | 3.047 / 1.965 |
| sky | 256.6 / 148.5 | -44.3% to +7.5% | 1.952 / 2.618 | 1.491 / 2.338 |
| translate | 182.7 / 140.2 | -51.8% to +41.6% | 2.617 / 3.099 | 2.236 / 3.383 |
| rotate | 146.7 / 166.6 | -59.8% to +48.7% | 3.336 / 2.399 | 2.712 / 2.567 |
| seam_edits | 122.0 / 122.3 | -10.4% to +32.3% | 4.185 / 4.736 | 2.891 / 2.863 |
| sky_moving | 187.1 / 258.6 | -15.3% to +83.5% | 2.714 / 2.225 | 1.447 / 1.258 |

The moving-sky median rises 38.2%, but one paired run declined and timing
variation is large. The stationary sky workload, which submits no terrain in
either revision, falls from 256.6 to 148.5 FPS. Other scenes also show observed
slowdowns. These results demonstrate removed submissions, **not a general or
isolated 38% gameplay FPS improvement**.

| Scene | P95 ms before / after | P99 ms before / after | 1% low FPS before / after |
| --- | ---: | ---: | ---: |
| underground | 6.828 / 7.058 | 10.466 / 8.742 | 80.0 / 104.8 |
| surface_still | 8.645 / 12.706 | 11.068 / 14.040 | 79.1 / 63.0 |
| overview | 11.679 / 8.659 | 13.085 / 12.965 | 68.3 / 58.8 |
| sky | 7.856 / 11.770 | 10.421 / 14.514 | 72.6 / 58.8 |
| translate | 8.892 / 12.645 | 10.550 / 15.718 | 83.9 / 55.9 |
| rotate | 11.605 / 9.992 | 14.085 / 12.100 | 64.4 / 79.3 |
| seam_edits | 13.641 / 14.561 | 16.053 / 20.183 | 58.1 / 47.1 |
| sky_moving | 8.917 / 6.358 | 11.825 / 8.670 | 72.7 / 103.1 |

These are average per-frame submissions, repeatable across all five trials:

| Scene | Draws before / after | Triangles before / after | Queries before / after |
| --- | ---: | ---: | ---: |
| underground | 25.00 / 25.00 | 16256.00 / 16256.00 | 0.00 / 0.00 |
| surface_still | 11.00 / 11.00 | 8466.00 / 8466.00 | 0.00 / 0.00 |
| overview | 45.00 / 45.00 | 22684.00 / 22684.00 | 0.00 / 0.00 |
| sky | 0.00 / 0.00 | 0.00 / 0.00 | 0.00 / 0.00 |
| translate | 41.30 / 41.23 | 23685.33 / 23682.75 | 41.30 / 41.23 |
| rotate | 42.55 / 41.50 | 24020.25 / 23165.94 | 42.55 / 41.50 |
| seam_edits | 42.00 / 42.00 | 22896.00 / 22896.00 | 42.00 / 42.00 |
| sky_moving | 2.00 / 0.00 | 1990.00 / 0.00 | 2.00 / 0.00 |

Seam edits still rebuild two chunks and upload 167,664 bytes in four buffer
uploads per frame. Median rebuild/upload CPU time rises from 1.513 to 2.227 ms;
building the new CPU bounds is included. Median warm renderer initialization
(meshing, images, uploads, and completion wait) rises from 237.700 to 258.220 ms.
Terrain generation is 37.311 / 40.260 ms. Initial geometry buffer payload stays
13,353,624 bytes, CPU block payload stays 4,194,304 bytes, and both builds own
256 terrain query objects. Other measured scenes rebuild and upload nothing.
Those byte counts exclude textures, allocation metadata, and driver overhead.

## Validation

The following passed after the rendering fix:

```text
Native Windows / Intel UHD Graphics:
  .\build.cmd -Test
  .\build.cmd -Configuration Debug -Test
  .\build.cmd -Benchmark
  .\tests\test_build.ps1

WSL Ubuntu 24.04 / Mesa and Xvfb:
  make -j4
  make test
  make test-sanitize
  make test-gl
  make CC=clang CFLAGS='-O2 -g -Werror' all test
  make CC=clang CFLAGS='-O2 -g -Werror' test-gl
  make test-build
```

All project C/H files under `src` and `tests` pass
`clang-format --dry-run --Werror`. Code review found no blocking issues.
These are CPU and hidden graphical/application checks; no hands-on interactive
playtest was performed. Tests use disposable worlds and temporary save paths.

To reproduce the performance comparison, archive `0a72efa`, copy the current
`tests/render_profile.h` into that archive, build both native Release benchmarks,
then run `tests/compare_render_profile.ps1` with their directories, a fresh
output directory, and `-Pairs 5`. The driver checks identical header hashes and
retains the environment, raw frame CSVs, logs, and per-run summaries.
