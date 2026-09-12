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

`src/graphics/shadows.c` owns two cached depth maps and their rendering resources.
A fixed orthographic projection encloses the entire
finite world in a sphere. It does not follow the camera, so moving or rotating
the view cannot shift the shadow sampling grid. Every nonempty chunk is a caster,
including chunks removed by the camera's frustum, distance, or occlusion tests.
The shadow pass uses filled polygons even when F4 displays terrain as wireframe.

Each depth map is 3072 by 3072, capped by the driver's maximum image dimension;
drivers below 1024 are rejected during initialization. Together, Depth24 storage
can use about 72 MiB with four-byte storage. Each map needs four nearest-depth
comparisons. The shader computes the receiver-plane depth separately at each
sampled texel and interpolates the resulting visibility. This avoids using one
reference depth for four different points on a sloped plane, which caused false
self-shadowing. A small normal offset handles depth precision. The result
softens shadow edges without simulating distance-dependent penumbrae or global
illumination.

The cache stores the two directions surrounding the current light angle on a
1024-step orbit. Their visibility blends continuously as time advances. Crossing
an interval reuses the shared endpoint and renders only the new one, roughly
once per 1.17 seconds of the normal cycle. This removes the rotating depth-grid
flicker and most repeated caster submission. Time commands select the appropriate
pair immediately; block edits invalidate both maps before reuse. Pausing retains
both maps and their blend weight. `RenderResult.shadowDrawCalls` counts new
caster submissions separately from visible terrain draws. A failed mesh upload
prevents rendering an incomplete update. See [measurements](shadow-performance.md).

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
unusual framebuffer and raster state. A low-sun roof across a chunk seam checks
long shadows, and intermediate light angles verify that edits refresh both maps.

`tests/shadow_motion_checks.h` holds the camera still for 180 frames while the
light moves, then pauses it for 20 frames. It compares a uniformly lit floor
against its expected brightness and checks a moving roof shadow for frame jumps.
The roof edge previously jumped 36/255 in one frame; the new result is 1/255.
The test requires actual shadow movement and contrast, so removing or freezing
the shadow cannot pass. It also bounds caster submissions and checks unchanged
pixels and zero shadow draws while paused.

The existing lighting fixture still compares all six faces at six positions
and three viewing angles. Material, occlusion, wireframe, sky, cloud, selection,
HUD, startup, and persistence regressions run through the normal test targets.

Run `.\build.cmd -Test` and `.\build.cmd -Configuration Debug -Test` on native
Windows, or `make test-gl` on Linux. For a focused graphical run, set
`KERNELCRAFT_ATMOSPHERE_CHECK=1` and run the benchmark target. Use `--no-save`
for disposable interactive sessions. Scripted rendering tests do not establish
physical input feel or visual quality on every driver.
