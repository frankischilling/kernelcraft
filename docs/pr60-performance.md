# PR #60 performance review

This pass reviews the first-party runtime code under `src/world`, `src/graphics`,
`src/utils`, `src/math`, the game loop, and the shaders. The baseline is
`954b61e` on `graphics/sky-position-glow`, including the initial cloud implementation.
The changes are implemented in PR #60; they are not a claim that the game has
reached a hardware-independent maximum frame rate.

## Changes retained

- Cloud occupancy is computed once and uploaded as 64 pairs of 32-bit words
  (512 bytes). Each visited cell needs a bit lookup instead of four hashed noise
  samples and their interpolation. Motion and the cloud silhouette are unchanged.
- Sky rendering skips zero-weight palettes and stops celestial calculations
  outside the existing 35-degree glow extent.
- Meshing caches block solidity with a one-cell border, skips face slices with
  no exposed faces, and returns early for empty chunks. The two deterministic
  greedy sweeps and material ordering remain intact. Temporary stack use grows
  by roughly 21 KiB for solidity, plus the slice flags.
- Tiny block validation predicates are inline, retaining rejection of invalid
  IDs. This permits optimization across their callers without enabling LTO or
  relaxing floating-point rules.
- Hidden F3 diagnostics no longer format strings or sample biome noise every
  frame. They refresh immediately when shown.

## Measurement method

Native Windows 11, Intel Core i7-13700H, Intel UHD Graphics driver
32.0.101.7077, MinGW GCC 13.2.0 Release (`-O2`), 1280x720, seed 0.
The NVIDIA GPU also present on this machine was not used. Three alternating
before/after pairs use the exact same profiling header, 120 warm-up frames and
600 measured frames per scene. Vsync is disabled. The pipelined path includes
completion of the final GPU batch and reads timestamp results afterward.

The old benchmark omitted the atmosphere. `KERNELCRAFT_PROFILE_ATMOSPHERE=1`
adds the actual sky, changing terrain light, cloud, selection, and HUD passes.
Day begins at phase 0.125; night at 0.625. Both use the same deterministic camera,
clock, and drift paths in each build. The additional `cloud_layer` scene flies
through the layer at Y=122. This measures rendering; it is not a keyboard/mouse
playtest or a benchmark of player physics, file I/O, or network traffic.

Results below use the median of the three trial means. The checked-in trial
CSVs retain frame medians, p95/p99, CPU submission, GPU timing, geometry,
uploads, and rebuild counts. Desktop scheduling introduces visible outliers;
small differences and individual tail measurements should not be treated as
universal gains. CPU-only mesh measurements use a monotonic high-resolution
timer, eight warm-ups, and 128 build/free iterations per trial.

## Measured results

Complete render-frame means in milliseconds; lower is better. These are the
original three-pair day and night sets, including the two noisy night results.

| Scene | Day before | Day after | Night before | Night after |
| --- | ---: | ---: | ---: | ---: |
| underground | 0.983 | 0.863 | 0.937 | 0.897 |
| surface_still | 0.878 | 0.756 | 1.030 | 1.094 |
| overview | 0.760 | 0.660 | 0.731 | 0.674 |
| sky | 0.783 | 0.704 | 0.770 | 0.561 |
| translate | 0.973 | 0.823 | 0.886 | 0.657 |
| rotate | 0.973 | 0.949 | 0.978 | 0.721 |
| seam_edits | 2.271 | 1.893 | 2.543 | 1.906 |
| sky_moving | 0.960 | 0.626 | 0.805 | 0.887 |
| wall_moving | 1.038 | 0.732 | 0.966 | 0.719 |
| cloud_layer | 1.084 | 0.714 | 0.809 | 0.702 |

Nighttime `surface_still` and `sky_moving` initially showed slower frame means
despite reduced GPU time. Five additional alternating pairs did not reproduce
those slowdowns: surface was 0.973 to 0.818 ms (16% lower), and moving sky was
0.974 to 0.634 ms (35% lower). Both sets are retained. Daytime rotation changed
by only 2.5%, which is too small to treat as a reliable improvement here.

With F3 visible, the surface scene changed from 1.395 to 0.937 ms (33% lower).
The measured startup mesh/build/upload stage changed from 153.546 to 94.637 ms
in the day trials (38% lower). Terrain generation itself is unchanged.

On edit frames, the two-chunk rebuild fell from 1.205 to 0.810 ms by day and
1.316 to 0.842 ms by night. Every render trial retained the same terrain draw,
triangle, surface-block, rebuilt-chunk and upload counts. The seam edit still
rebuilds two chunks, makes four uploads, and sends 167,664 bytes per edit frame.

CPU-only build/free means:

| Chunk | Before ms | After ms |
| --- | ---: | ---: |
| generated | 0.468 | 0.317 |
| empty | 0.202 | 0.010 |
| sparse | 0.358 | 0.053 |
| solid | 0.691 | 0.256 |
| checkerboard | 3.095 | 3.021 |

Empty and sparse chunks benefit most. The checkerboard changes by only about
2%, so no meaningful gain is claimed for that output-heavy case. Vertex, index,
and surface-block counts agree between builds in all five fixtures.

Raw trial summaries and machine/compiler context are in
[`docs/benchmarks/pr60-2026-09-10`](benchmarks/pr60-2026-09-10).

## Validation

`./build.cmd -Test` and `./build.cmd -Configuration Debug -Test` passed on native
Windows, including the application, persistence, HUD, CPU and real OpenGL tests.
The original and optimized cloud masks share fingerprint 12639864285259775753
across all 4,096 cells. All nine day/dawn/dusk/night/celestial/above-cloud captures
are byte-identical between the baseline and the final Debug build. Existing
independent unit-face coverage checks validate mesh contents and ordering.

The standalone-chunk regression was run against the baseline and failed its
six-rectangle cuboid assertion before passing with the optimization. Formatting
and diff checks pass. The live [PR checks](https://github.com/frankischilling/kernelcraft/pull/60)
record Linux GCC/Clang, sanitizers, Mesa application/rendering and build-regression
results for the published revision. Linux was not run locally.

## Review coverage and remaining limits

| Area reviewed | Decision |
| --- | --- |
| Sky, clouds, shader initialization and assets | Cache invariant cloud work; avoid invisible sky work. Keep colors, sampling, resource ownership and GL state restoration. |
| Terrain submission, frustum, mesh visibility and software occlusion | Retain current conservative visibility and cached draw lists. A surface hierarchy needs separate evidence from larger worlds. |
| Mesh construction and block predicates | Cache local solidity, skip empty slices, retain exact geometry and invalid-ID behavior. Checkerboards still require large output buffers. |
| HUD, chat panels and text cache | Skip hidden diagnostic preparation. Existing cached glyphs remain; a batched modern HUD could reduce compatibility-mode calls but requires a larger rendering change. |
| Generation, Perlin/math and coordinates | Preserve seed output and arithmetic. Noise reuse and bounded column filling are possible startup refinements, below mesh/upload cost in this workload. |
| Input, camera, player physics, edits and ray selection | Work is bounded; retain contact rounding, swept collision, crouch/run behavior and pause checks. |
| Persistence, command-line options, chat and cycle timing | Retain validation, transactional save replacement and bounded input. No continuous heavy workload demonstrated. |
| Build/test integration and third-party libraries | Extend existing harnesses; retain the toolchain and GL 3.3 compatibility. Bundled third-party implementations were not modified or exhaustively audited. |

The solidity cache also fixes detached chunk meshing: interior neighbors now
come from the supplied chunk instead of an unrelated live world. A regression
with two adjacent blocks and an invalid-ID neighbor fails on the baseline and
passes with the cache. No save format, terrain content, collision rule, render
distance, resolution, or visual-quality setting changed.

## Reproduction

Build the baseline and candidate in separate directories with the identical
`tests/render_profile.h`. Then, in PowerShell:

```powershell
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
./tests/compare_render_profile.ps1 -BaseDirectory <baseline> -CandidateDirectory <candidate> -OutputDirectory <new-day-directory> -Pairs 3 -Pipelined
$env:KERNELCRAFT_PROFILE_PHASE = '0.625'
./tests/compare_render_profile.ps1 -BaseDirectory <baseline> -CandidateDirectory <candidate> -OutputDirectory <new-night-directory> -Pairs 3 -Pipelined
```

Use `KERNELCRAFT_PROFILE_DEBUG=1` for F3, or `KERNELCRAFT_PROFILE_SCENE` to select
one scene. `tests/mesh_profile.c` links against the CPU world test sources; use
the same file and compiler flags for both revisions. For example on Linux:

```sh
cc -std=c11 -O2 -Isrc tests/mesh_profile.c src/world/*.c src/math/math.c src/graphics/frustum.c src/utils/raycast.c -lm -o /tmp/kernelcraft-mesh-profile
/tmp/kernelcraft-mesh-profile
```

The Windows measurements compile those same sources with native GCC and use
`QueryPerformanceCounter`; the Linux helper uses `CLOCK_MONOTONIC`. No saves
are read or written by either benchmark.
