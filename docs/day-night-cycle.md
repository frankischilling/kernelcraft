# Day/night cycle

The cycle lasts 20 minutes of active gameplay. It starts at tick 3,000
(morning), advances at 20 ticks per second, and wraps after 24,000 ticks.
Tick 0 is sunrise, 6,000 is noon, 12,000 is sunset, and 18,000 is midnight.
Rendering interpolates the fractional tick so celestial movement is smooth.
The sun rises in +X and sets in -X; the full moon follows the opposite half
of the same orbit. Both appear behind terrain.

Chat entry, Escape/cursor release, focus loss, minimization, and a zero-sized framebuffer
pause the clock. The first resumed frame ignores elapsed pause time. Active
frame stalls advance at most 0.1 seconds, matching the existing bounded
simulation behavior. There is no wall-clock catch-up while the game is closed.
Every launch starts in the morning, including when loading a saved world.
The save format and existing worlds are unchanged; saving cycle time is a
separate TODO.

The [local chat](local-chat.md) supports `/time set day`, `/time set night`,
and `/time set 0` through `/time set 23999` to change the live cycle.

The three supplied palettes blend smoothly with solar elevation. Day and
night are fully established when their respective body is about 20 degrees
above the horizon; the dawn/dusk palette is exact at sunrise and sunset.
Day and night retain the same 24-degree spacing and smooth interpolation.
Day's five color heights are -30, -6, 18, 42, and 66 degrees: one band lower
than night's -6, 18, 42, 66, and 90 degrees. This narrows the pale daytime
horizon strip and gives cyan and blue more visible coverage. The true
horizon blends the second and third day colors, while the strongest blue
is reached at 66 degrees. Their PNG rows run in opposite orders, so the
shader reverses night's sampling order to keep blue and purple overhead.
Dawn/dusk retains night's heights with orange just below the horizon.
Every swatch color is retained unchanged.
Sky colors are display colors and bypass terrain lighting. Terrain diffuse light follows
the sun or moon, with warm twilight fill and dim purple-blue night fill.
The directional intensity fades to zero at the horizon before changing
bodies. Night retains enough ambient light to navigate. This remains
unshadowed lighting: enclosed rooms receive the same fill as exposed faces.

Stars form a deterministic decorative field fixed to world directions.
They fade in after sunset, fade out before sunrise, and soften near the
horizon. Camera translation does not move the field. This is not an
astronomical simulation. Advanced realistic star positions, constellations,
and apparent motion are in the TODO list, along with the remaining moon
phases once their artwork is ready. The current moon is always full.

The full moon has a white pixel-stepped halo; the sun has a yellow-orange
halo using the same square profile. A coarse mask in body coordinates gives
both glows crisp steps around the original artwork. A soft bloom skirt and
a broader forward-scattering lobe soften the transition back into the sky.
The scattering uses a peak-normalized Henyey-Greenstein approximation with
`g = 0.8`, strengthened near the horizon and faded out by 35 degrees from
each body. The sun's scattering is stronger than the moon's.

This is a compact sky-pass approximation of celestial scattering and bloom,
not a volumetric atmosphere or full-scene HDR bloom pipeline. It draws on
the separate sun/moon scattering controls described in Minecraft's
[Atmospheric Effects documentation](https://learn.microsoft.com/en-us/minecraft/creator/documents/vibrantvisuals/atmosphericscustomization)
and the [Henyey-Greenstein phase function](https://www.pbr-book.org/4ed/Volume_Scattering/Phase_Functions).
The stepped mask is computed in the shader; the supplied artwork remains
the source for each visible body.

Glow size is angular, so it follows the same perspective as the bodies when
looking around, changing field of view, or resizing. The sky stays centered
on the camera during movement. Lowering the color gradient does not shift
the orbit or the physical horizon: stars, bodies, and glows still fade at
zero elevation, and terrain covers the sky and halos.

`src/world/day_night.c` owns timing and phase/light sampling without graphics
dependencies. `src/graphics/sky.c` draws one full-screen triangle before
terrain, without writing depth. `setWorldDayNight` updates cached terrain
uniforms; changing time never rebuilds chunk meshes or changes culling.
The sky owns its shader, vertex array, and five images and releases them
while the GL context is current. The HUD and selection retain their own
rendering paths.

## Clouds

Clouds form connected, square-edged patches in a four-block-thick layer from
Y=120 to Y=124. Each cell is 12 blocks wide. Tops are white in daylight;
undersides and sides are shaded. They turn warm at twilight and dark blue-gray
at night. Clouds have world parallax and drift in +X at 0.6 blocks per second.
Their deterministic pattern repeats every 768 blocks, including across negative
coordinates. Drift pauses under the same conditions as the cycle, ignores the
first resumed frame, and limits stalls to 0.1 seconds. `/time set` changes
their lighting without jumping their position. Drift resets on launch.

`src/graphics/clouds.c` draws after terrain and selection, before the HUD.
Its shader traces the first occupied cell in the horizontal layer, including
views from above and inside clouds. Projected hit depth lets nearby terrain
hide clouds; the pass blends over the sky without writing terrain depth.
Clouds are 92% opaque nearby and fade between 480 and 768 blocks from the eye.
This is a decorative cloud layer with no collision, weather, or cloud shadows.
The bounded shader traversal uses one full-screen triangle and creates no
terrain meshes. Supplied sky colors and celestial artwork are unchanged.
The repeating occupancy pattern is calculated once at initialization and sent
to the shader as a 512-byte bitset. Rendering reads those bits rather than
recalculating noise for every visited cell. The sky also skips inactive palette
samples and celestial glow calculations outside their visible extent. See the
[PR #60 performance measurements](pr60-performance.md).

## Checks

The CPU world suite includes timing at 20/60 frames per second, full-cycle
wraparound, pause/resume, invalid elapsed values, bounded stalls, phase
weights, orbit directions, and continuity. The graphical benchmark checks
all five bands of all three palettes against independently recorded RGB
values, day/night midpoint blends, the cyan daytime horizon, nighttime star pixels, repeatability after camera translation,
sun/full-moon visibility, halo colors and falloff, square halo shape and pixel steps,
forward glare beyond the pixel halo, original body colors,
perspective alignment in landscape and portrait views, horizon clipping,
halo occlusion, untouched depth, GL state restoration, and dimmer terrain
without mesh uploads. The application harness checks the live sky
pass and pause/resume alongside movement, editing, wireframe, HUD, and
normal shutdown. Its wireframe and selection checks compare against the
actual sky background.

Cloud framebuffer checks cover patches and gaps, darker night lighting, visible
tops, world parallax, drift, seamless negative-coordinate repetition, near/far
depth planes bracketing the cloud surface, views inside the layer and along the
horizon, untouched depth, and restored GL state. Timing checks cover pauses,
invalid elapsed values, stalls, and wraparound. The application harness also
compares live cloud pixels against the sky before the HUD is drawn. Optional
`KERNELCRAFT_CLOUD_CAPTURE` captures day, dusk, night, and above-cloud views.

These are scripted CPU and OpenGL checks, not a physical keyboard/mouse
playtest or a claim about appearance on every display.
