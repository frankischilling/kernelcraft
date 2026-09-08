# Block highlighting

## Edges touching neighboring terrain

The user's clarified report and
[reference screenshot](https://www.9minecraft.net/wp-content/uploads/2024/10/Rainbow-Outlines-Animation-Screenshots-2.png)
identify a separate failure from the close-up case below: the bottom border
disappears into the floor, and a side border can disappear into an adjoining wall.
This fix starts at `da4bd89fef2cf21ee5082e7969d58c0952a0d974` on
`fix/neighbor-selection-outline`, addressing
[issue #28](https://github.com/frankischilling/kernelcraft/issues/28).
There were no open PRs when work started.

The wireframe previously expanded 0.003 units outside the selected voxel.
Those expanded edges could lie inside another solid block. The outline now
uses exact face boundaries with polygon line offset to separate its depth from
the terrain. The slope contribution is capped at 0.0005 of the depth range;
uncapped slope offset can pull nearly edge-on borders through foreground blocks.
Four-corner faces retain just the borders, without triangle
diagonals. All visible faces receive an outline, including edges shared with
the floor and neighboring blocks. Foreground terrain still occludes the
selection; the face tint and editing behavior are unchanged.

The new framebuffer regression renders a block on a floor beside a flush
neighbor, then beside a wall projecting toward the camera. Each arrangement
is rotated onto six faces and viewed from three angles: 36 views total.
It checks the aimed face's four borders and, in the first arrangement, the
other three top borders visible in the reference. Before the fix, the bottom
border had 0/32 visible samples and a side border fell to 12/32. All views pass
afterward on Mesa llvmpipe and native Intel UHD Graphics. Before/after frame
captures also show the missing bottom line restored.

Read-only review found the excessive slope offset in the initial implementation.
A taller foreground wall reproduced it at four of five nearly coplanar top/side
views on Mesa; an independent Intel check also reproduced it. The bounded slope
offset passes all five views and keeps the 36 neighbor views passing. Each
foreground comparison requires the entire framebuffer to remain unchanged.
Follow-up review independently reran the rebuilt native Intel benchmark and
found no remaining actionable issue.

These commands passed in `C:/Users/imike/kernelcraft-neighbor-outline`:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/imike/kernelcraft-neighbor-outline && make -j4 all test test-gl'
.\build.cmd -Test
wsl -d Ubuntu -- env MESA_GL_VERSION_OVERRIDE=3.3COMPAT MESA_GLSL_VERSION_OVERRIDE=330 bash -lc 'cd /mnt/c/Users/imike/kernelcraft-neighbor-outline/bin/linux/Release && xvfb-run -a ./benchmark'
```

Linux used GCC 13.3.0 and native Windows used MinGW64 GCC 13.2.0. The suites
cover CPU behavior, application startup/input/editing, persistence, textures,
outlines, close-up highlighting, foreground occlusion, and GL state restoration.
Logs are `%TEMP%/kernelcraft-neighbor-{baseline,red,linux-bounded,windows-bounded,gl33}.log`.
The review regression is in `kernelcraft-neighbor-grazing-{red,bounded}.log`.
Passing a filename prefix to the built `benchmark` executable also writes
`PREFIX-selection-neighbors.ppm` for visual comparison.

The worktree keeps the main checkout's in-progress texture edits and user save
separate from the tests. The requested skill manifests, Git helper, and verified
identity are the same as the [workflow record](windows-incremental-build.md#workflow-and-next-milestone).
To exercise the fix, build in this worktree with `.\build.cmd`, then run
`.\bin\windows\Release\minecraft_clone.exe --no-save`. Aim at a block resting
on terrain, place another beside it, and inspect the bottom and side borders
while moving around it. Physical interactive playtesting remains unverified;
[issue #9](https://github.com/frankischilling/kernelcraft/issues/9) still tracks
that foundation acceptance work.

## Close-up faces

Base: `a701c018b0762be5989d04c1405c53d86d25c4c0`, after the requested merge of
[PR #25](https://github.com/frankischilling/kernelcraft/pull/25). Work is on
`fix/selection-outline-visibility`, addressing
[issue #26](https://github.com/frankischilling/kernelcraft/issues/26).

The reproduced failure occurs when looking up inside a two-block-high passage.
The eye sits 0.38 units below the ceiling. At the application's 70-degree field
of view and 16:9 aspect ratio, the selected face fills the viewport and every
wireframe edge is clipped. Selection still hits the block, but the outline
cannot show it. Ordinary underside and diagonal views passed before this fix;
the user's exact intermittent viewing situation remains unconfirmed.

The selected face now receives a subtle gold tint in addition to the existing
outline. Its two triangles use the exact voxel face bounds. A small polygon
offset separates the coplanar overlay from the surface, while depth testing
keeps nearer terrain in front. This follows the
[OpenGL polygon-offset behavior](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glPolygonOffset.xhtml).
The overlay leaves depth storage untouched and restores its blend, polygon,
enable, color, depth, program, and matrix state. No per-frame buffer allocation
or upload is added. Editing and ray traversal retain their existing behavior.

The close-up regression failed with 0/4096 highlighted central pixels before
the fix. It now passes with 4096/4096 on all six faces. Further pixel tests
check ordinary outlines at nine diagonal angles on each face, unchanged
neighboring pixels, identical depth samples, complete foreground occlusion,
misses, and restoration of deliberately different caller GL settings.
Matrix mode is tested explicitly; matrix contents are restored through the
existing push/pop pairs. Read-only review found no actionable issue.

These local commands passed:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/imike/kernelcraft && make -j4 all test test-gl'
.\build.cmd -Test
wsl -d Ubuntu -- env MESA_GL_VERSION_OVERRIDE=3.3COMPAT MESA_GLSL_VERSION_OVERRIDE=330 bash -lc 'cd /mnt/c/Users/imike/kernelcraft/bin/linux/Release && xvfb-run -a ./benchmark'
```

Linux used GCC 13.3.0 and Mesa llvmpipe LLVM 20.1.2. Native Windows used MinGW64
GCC 13.2.0 and Intel UHD Graphics. CPU, shader/texture, HUD, running-application
input/edit/collision, persistence, and rendering checks passed. Formatting and
`git diff --check` passed. Local Debug, sanitizer, and build-system regressions
were not rerun for this rendering change; hosted GCC/Clang checks remain part
of the PR workflow. Logs are `%TEMP%/kernelcraft-selection-{close-red,gcc,windows,gl33}.log`.

To exercise it, run `.\build.cmd -Run`, use a new explicit `--world` path, and
look up beneath a two-block-high ceiling. The selected face should remain
lightly tinted even when no border fits on-screen. Physical playtesting of
the user's original situation is still needed. Existing user saves and the
unrelated local dirt-texture edit were preserved. The requested skill/helper
paths and configured identity are recorded in the
[preceding workflow checkpoint](windows-incremental-build.md#workflow-and-next-milestone).
