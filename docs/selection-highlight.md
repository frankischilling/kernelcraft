# Close-up block highlighting

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
