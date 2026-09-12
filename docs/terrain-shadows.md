# Terrain shadows and nighttime lighting

Terrain now casts shadows from the active sun or moon. Roofs and terrain outside
the camera view can block direct light, and edits update the shadow on the next
successful frame. `/time set day`, `/time set night`, and numeric time commands
change the direction with the existing day/night cycle. The shadow only reduces
direct light; hemispheric fill remains visible under roofs and in shadow.

Nighttime light uses cool blue fill with more brightness on every face. The
graphical fixture's 180/255 gray block reads 88/255 on top, 56/255 on a side,
and 46/255 underneath at midnight on Intel UHD Graphics. The same baseline
samples were 60, 33, and 26. Noon still reads more than twice as bright on top.

## Rendering

`src/graphics/shadows.c` owns a depth-only framebuffer, comparison-sampled depth
image, and shader program. A fixed orthographic projection encloses the entire
finite world in a sphere. It does not follow the camera, so moving or rotating
the view cannot shift the shadow sampling grid. Every nonempty chunk is a caster,
including chunks removed by the camera's frustum, distance, or occlusion tests.
The shadow pass uses filled polygons even when F4 displays terrain as wireframe.

The depth image is 4096 by 4096, capped by the driver's maximum image dimension;
drivers below 1024 are rejected during initialization. Depth24 storage can use
about 64 MiB at the full size. The shader uses nine half-texel-spaced comparison
samples with bilinear filtering. A receiver-plane depth correction and a small
normal offset prevent a face from shadowing itself at oblique light angles.
The result softens the shadow edge; it does not simulate distance-dependent
penumbrae or global illumination.

The map is reused while normalized light direction and chunk geometry are
unchanged. An advancing clock refreshes it each frame, submitting all nonempty
chunks again. `RenderResult.shadowDrawCalls` counts those additional draws
separately from visible terrain draws. This adds GPU work and memory; no speedup
is claimed. A failed mesh upload prevents rendering an incomplete update.

The module restores separate draw/read framebuffer bindings, viewport, program,
vertex array, depth settings, polygon modes, and the enable flags it uses. GPU
resources are released before context destruction, including startup failures.
Sky, clouds, selection, and HUD geometry do not cast terrain shadows. Clouds
use their own [transparent volume pass](cloud-transparency.md). All existing
block materials remain opaque; transparent voxel materials still need material
visibility and render-order rules. World generation, collision, and saves retain
their existing behavior and formats.

## Checks

`tests/atmosphere_render_checks.h` renders a block below an off-screen roof.
The baseline gave identical 181/255 samples with and without the roof. The
new result is 126/255 in shadow and 181/255 with the roof removed or the light
reversed. Tests also cover moon shadows, unchanged-map reuse, edit/time refresh,
midnight face readability, finite-world corner coverage, and restoration from
unusual framebuffer and raster state.

The existing lighting fixture still compares all six faces at six positions
and three viewing angles. Material, occlusion, wireframe, sky, cloud, selection,
HUD, startup, and persistence regressions run through the normal test targets.

Run `.\build.cmd -Test` and `.\build.cmd -Configuration Debug -Test` on native
Windows, or `make test-gl` on Linux. For a focused graphical run, set
`KERNELCRAFT_ATMOSPHERE_CHECK=1` and run the benchmark target. Use `--no-save`
for disposable interactive sessions. Scripted rendering tests do not establish
physical input feel or visual quality on every driver.
