# Day/night cycle

The cycle lasts 20 minutes of active gameplay. It starts at tick 3,000
(morning), advances at 20 ticks per second, and wraps after 24,000 ticks.
Tick 0 is sunrise, 6,000 is noon, 12,000 is sunset, and 18,000 is midnight.
Rendering interpolates the fractional tick so celestial movement is smooth.
The sun rises in +X and sets in -X; the full moon follows the opposite half
of the same orbit. Both appear behind terrain.

Escape/cursor release, focus loss, minimization, and a zero-sized framebuffer
pause the clock. The first resumed frame ignores elapsed pause time. Active
frame stalls advance at most 0.1 seconds, matching the existing bounded
simulation behavior. There is no wall-clock catch-up while the game is closed.
Every launch starts in the morning, including when loading a saved world.
The save format and existing worlds are unchanged; saving cycle time is a
separate TODO.

The three supplied palettes blend smoothly with solar elevation. Day and
night are fully established when their respective body is about 20 degrees
above the horizon; the dawn/dusk palette is exact at sunrise and sunset.
The five colors in each swatch form a smooth gradient from zenith to horizon.
Daylight blue and dark night purple sit overhead; dawn/dusk orange sits at
the horizon. Each gradient passes through the original swatch colors. Sky colors are
display colors and bypass terrain lighting. Terrain diffuse light follows
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

`src/world/day_night.c` owns timing and phase/light sampling without graphics
dependencies. `src/graphics/sky.c` draws one full-screen triangle before
terrain, without writing depth. `setWorldDayNight` updates cached terrain
uniforms; changing time never rebuilds chunk meshes or changes culling.
The sky owns its shader, vertex array, and five images and releases them
while the GL context is current. The HUD and selection retain their own
rendering paths.

## Checks

The CPU world suite includes timing at 20/60 frames per second, full-cycle
wraparound, pause/resume, invalid elapsed values, bounded stalls, phase
weights, orbit directions, and continuity. The graphical benchmark checks
all five bands of all three palettes against independently recorded RGB
values, nighttime star pixels, repeatability after camera translation,
sun/full-moon visibility, untouched depth, GL state restoration, and dimmer
terrain without mesh uploads. The application harness checks the live sky
pass and pause/resume alongside movement, editing, wireframe, HUD, and
normal shutdown. Its wireframe and selection checks compare against the
actual sky background.

These are scripted CPU and OpenGL checks, not a physical keyboard/mouse
playtest or a claim about appearance on every display.
