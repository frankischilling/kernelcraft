# Terrain lighting

The original lighting checkpoint below describes the fixed reference setup.
The game now uses the [day/night cycle](day-night-cycle.md) to update the
world-space light direction and fill each frame. Its tests retain the fixed
reference setup to isolate material mapping and camera-independent shading.

Terrain uses a world-space light direction with matte diffuse shading.
Soft hemispheric fill keeps side faces and undersides readable, with a slightly
cooler fill above and warmer fill below. Moving through the map or turning the
camera no longer moves a glossy highlight or changes an identical face's light.
The live game follows the visible sun or full moon as its clock advances.

The fragment shader decodes the existing RGBA8 texture samples from sRGB,
multiplies the linear color by the lighting, and encodes the result for the
display framebuffer. The fixed intensities leave headroom for bright texture
detail. `GL_FRAMEBUFFER_SRGB` stays disabled: enabling it would encode the terrain
twice and would also change the existing HUD and selection colors. Texture
files, array formats and layer ordering, nearest filtering, repeated UVs, and
deterministic variants are unchanged.

`src/graphics/world_renderer.c` initializes the light uniforms and updates them
when the cycle advances. The shader uses the face normal, so greedy rectangles
need no additional vertices or merge constraints. Camera position still controls
culling and projection, but is no longer a shader lighting input. The grid,
selection overlay, hotbar, and F4 wireframe controls retain their existing
rendering paths. World generation and save formats are unchanged.

Opaque terrain now casts filtered directional shadows. A 2048 by 2048 depth
map is rendered from a stable orthographic sun or moon view, using all non-empty
terrain chunks so shadows can fall across chunk boundaries. The terrain shader
uses a 3 by 3 percentage-closer filter and a slope-scaled depth bias. The map is
refreshed after dirty chunk uploads and when the active light direction changes;
unchanged frames reuse it. Ambient hemispheric fill remains present in shadowed
areas, and clouds, selection, and HUD geometry do not cast terrain shadows.
Day/night simulation and sun/full-moon rendering remain implemented in the cycle
module.

The nighttime balance keeps blocks readable under the moon and in shadowed areas:
the moon diffuse term and blue-purple sky/ground fills are raised together. The
graphical fixture requires a uniform 180/255 gray stone top to reach at least
72/255 at midnight and a side to reach at least 40/255, while the noon top stays
more than twice as bright.

## Regression checks

`tests/lighting_render_checks.h` runs through the real terrain renderer in the
existing graphical benchmark. A uniform tile isolates lighting from material
variation. The fixture compares all six faces at six positions, including
negative coordinates, either side of a chunk seam, and the old point-light
position, with three camera angles per face. It checks readable undersides,
distinct face brightness, black preservation, highlight headroom, and decoded
gray/white reflectance ratios at seven texture levels.

The new regression failed against the previous shader with 123 unstable channel
samples. A mid-gray tile produced 26/255 on dark faces and 217/255 on top in the
initial fixture. The fixed shader produces approximately 49-55 underneath,
68-97 on sides, and 120-122 on top, with the same results across the tested
positions and camera angles.

The shadow fixture renders a floor and an edited six-block column with the real
terrain renderer. It checks that a positive-X light darkens the left receiver,
that removing the column refreshes the depth map, and that reversing the live
day/night light moves the shadow to the right receiver. It also checks stable
frames, initial shadow submissions, and restoration of the caller's framebuffer,
depth, blend, cull, scissor, polygon, and clear-depth state.

The independent material fixture uses separate 2D textures and tabulated face
irradiance, keeping texture mapping separate from the production texture array
and diffuse calculation. Its nine material arrangements and six views continue
to check UV repetition and per-voxel variants. Existing tests also cover dirty
seam updates, selection pixels, wireframe state, HUD, input, and save/restart.

Run `make test-gl` on Linux or `.\build.cmd -Test` on Windows to include these
checks. Scripted OpenGL tests and captured views do not establish physical
keyboard/mouse behavior or visual quality on every display.

## Validation record

Base: `774e33ad41a4dfd6bb161f81e4c939608aa2b5d1` on `main`. There were no open
pull requests before this work began. The baseline Windows Release suite and
Linux build/CPU suite passed. The new lighting regression then failed with exit
23 against the old shader before the fix.

The following checks passed with exit 0 after the code changes:

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
clang-format --dry-run --Werror src/graphics/world_renderer.c tests/render_benchmark.c tests/lighting_render_checks.h
```

```sh
make -j4 all test test-gl
make test-sanitize
make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl
make test-build
```

Windows used native MinGW GCC and Intel UHD Graphics. Linux used Ubuntu under
WSL, GCC/Clang, and Mesa llvmpipe through Xvfb, with `LIBGL_ALWAYS_SOFTWARE=1`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The deliberate shader,
buffer-upload, and save-failure diagnostics are expected fixture output.
All 54 material/view comparisons reported zero differing pixels on both drivers.

Fixed-camera Windows captures were inspected before and after the shader change.
No physical keyboard/mouse playtest, monitor-scaling check, or Windows sanitizer
run was performed. No performance improvement is claimed. The existing user
save and unrelated working-tree changes were preserved.
