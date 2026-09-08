# Terrain variants and render distance

Base: `165fd4f`, after the outline fix in PR #29 was merged. There were no open
PRs at the start of this increment. Work is on `feat/terrain-variants-distance`,
addressing [issue #30](https://github.com/frankischilling/kernelcraft/issues/30).

The supplied artwork now appears as occasional alternatives:

| Surface | Ordinary tile | Alternate tile | Approximate frequency |
| --- | --- | --- | --- |
| Grass sides | `grass-side.png` | `grass-bug.png` | 2% |
| Grass tops | `grass-top.png` | `grass-top-leaves.png` | 10% |
| Dirt, including grass undersides | `dirt.png` | `dirt-rocks.png` | 25% |

The updated ordinary dirt and grass-side assets are included with the three
alternate images. All are supplied 16 by 16 PNGs. Stone keeps its existing tile.
These are visual variants of existing block types, with the same editing,
collision, hotbar, and save behavior.

## Rendering

The texture array has seven layers. Meshes retain the four base material IDs;
the fragment shader chooses a variant for each voxel inside a greedy rectangle.
Texture coordinates still repeat once per block. There are no additional
per-block submissions, mesh splits, or idle buffer uploads.

The choice uses a 32-bit integer hash of world seed, signed block coordinates,
and base material. Each family uses its own hash input. Grass sides on the same
block agree, while the top and underside have independent choices. A small
inward normal offset identifies the owning voxel on both sides of every axis.
The shader receives the block size and seed when world rendering initializes.
This makes selection stable across chunk boundaries, remeshing, and reloading
the same saved world. It also applies to existing saves without migration.

The horizontal chunk-center radius increases from two to six chunks: 32 to
96 blocks. Frustum culling remains active, and world dimensions remain
256 by 64 by 256. Whole chunks enter and leave the radius as before. In the
standard seed-zero benchmark, the level view now submits 42 terrain chunks
instead of 8; the elevated downward view submits 48. More terrain means more
drawing work. This is a distance increase, with no frame-rate improvement claim.

## Verification

The distance regression failed before the change because a block 64 units away
produced no pixels or submitted chunk. It now checks visible blocks at 64 and
80 units and culling at 112 units, in both directions. The variant regression
initially failed because only four texture layers were loaded.

The GPU fixture renders 32 by 32 block panels for all six faces of grass, dirt,
and stone at seed 0, seed 4294967295, and seed 0 again. Temporary solid-color
array layers identify the actual shader-selected layer at each of 55,296 tile
centers. These exercise negative coordinates and chunk seams, with an independent
C oracle. Rebuilding edited panels leaves the framebuffer unchanged; returning
to the first seed reproduces its fingerprint. The sampled variant frequencies
were 25.69% for dirt, 8.56% for tops, and 1.89% for sides. These are finite sample
counts, not exact quotas for each patch of terrain.

Separate framebuffer comparisons use the actual PNGs with a sampler2D unit-cube
reference. They cover grass, dirt, stone, and mixed prisms on all six faces,
checking repeated UVs, orientation, and layer mapping. Existing outline,
foreground-occlusion, application, and persistence fixtures remain included.

Validation commands from `C:/Users/imike/kernelcraft`:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/imike/kernelcraft && make -j4 all test test-gl'
.\build.cmd -Test
wsl -d Ubuntu -- env MESA_GL_VERSION_OVERRIDE=3.3COMPAT MESA_GLSL_VERSION_OVERRIDE=330 bash -lc 'cd /mnt/c/Users/imike/kernelcraft/bin/linux/Release && xvfb-run -a ./benchmark'
```

All three commands passed. Linux used GCC 13.3.0 and Mesa llvmpipe LLVM 20.1.2;
native Windows used MinGW64 GCC 13.2.0 and Intel UHD Graphics. Both produced
identical variant counts, and all 24 actual-art comparisons matched. The seed-zero
world still has 79,481 quads and 13,352,808 bytes of vertex/index payload.
Temporarily disabling variant selection in the built shader made the GPU test
fail at a grass side expecting the bug layer; the shader was restored before
the successful GL 3.3 run. Formatting and whitespace checks passed. Read-only
code review found no actionable issue; that reviewer did not run the suites.
Logs use `%TEMP%/kernelcraft-variants-{red,layers-red,green,linux,windows,gl33}.log`.
Build scripts and world/save formats are unchanged. The requested skill manifests,
Git helper, and verified identity are recorded in the
[workflow record](windows-incremental-build.md#workflow-and-next-milestone).

Build and run with `.\build.cmd -Run` or `make run`. For a disposable preview,
use `.\bin\windows\Release\minecraft_clone.exe --no-save`. Look across terrain
in flight mode, then inspect grass walls, grass tops, and dirt at close range.
Physical interactive playtesting remains unverified; foundation acceptance is
still tracked by [issue #9](https://github.com/frankischilling/kernelcraft/issues/9).
