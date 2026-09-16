# Player skins and camera views

The player uses the supplied `art/skin.png` through its identical runtime copy
at `src/assets/player/skin.png`. The six cuboids have separate base and optional
outer layers. The [artwork layout](../art/README.md#player-skin) specifies every
face rectangle of the 64×64 RGBA sheet, including the distinct left limbs.

F6 cycles first-person, rear third-person, and front third-person views. A third-person
camera normally sits three blocks from the eye. Center, edge, and corner probes
limit its distance near terrain. When less than 0.8 blocks remain, that frame
uses first-person rendering. This does not change the selected camera mode.
F6 ignores repeats, chat entry, inactive/minimized windows, and zero framebuffer
sizes. The chosen mode survives pauses and resets to first person on restart.

The original eye camera still controls movement, aiming, block selection,
placement, breaking, and the saved yaw/pitch. Third-person front view is an
inspection view: editing still follows the player's aim, away from that display
camera. The model and display camera do not change collision dimensions, standing
or crouched eye heights, reach, block-breaking durations, or save formats.

## Model and animation

`world/player_model.c` holds the CPU-only skin rectangles, dimensions, joints,
and poses. Local X points to the wearer's right, Y points up, and the front
faces negative Z. The standing model is 32 skin pixels high, scaled to the
existing 1.8-block player height. Arms and legs are four pixels wide; the torso
is eight pixels wide. Shoulders and hips rotate independently, and the head
follows the viewing pitch.

Walking phase advances from actual horizontal displacement on credited physics
ticks. Running increases the swing; standing against a wall stops phase advance
and eases the limbs toward rest. Airborne movement has a separate pose. Crouching
uses a smaller render scale and a bent posture to follow the existing one-block
body height. The fully posed outer shell is fitted between feet and ceiling,
including head pitch and limb swing. These poses are presentation only; physics retains its existing
fixed-step simulation and immediate posture changes.

Punches use a repeating 0.35-second visual cycle sampled from the existing
`BlockBreaking.elapsed` value. They do not accumulate another gameplay timer.
Resetting/cancelling the break returns the hand to rest, including release,
changed slots, right-click, flight transitions, pause, and target/reach changes.
Held breaking still removes a block only when the original material duration
has elapsed. Pauses clear gait and punch state without a resumed-frame jump.

## Rendering

`graphics/player_renderer.c` owns one static vertex buffer and one RGBA8
`GL_TEXTURE_2D`. Skin sampling uses nearest filtering, clamp-to-edge, and no
mipmaps. The source must decode as exactly 64×64 with four channels. Missing,
incorrectly sized, or invalid assets fail initialization and release partial GPU
resources while the context remains current.

The body draws after terrain/sky and before selection/clouds with ordinary
depth testing. Fully transparent skin fragments discard. Opaque outer-layer
fragments write depth; fractional outer alpha blends without changing depth.
The first-person pass draws the same right-arm mesh and sleeve in camera space
after clouds and before the HUD. It preserves the world depth buffer so nearby
terrain cannot cut through the hand and the HUD remains independent.

Both player passes draw filled geometry even when F4 makes the terrain wireframe,
then restore the OpenGL state they changed. The skin uses its own texture/shader
path; it does not add layers or materials to the terrain array. Existing cached
terrain shadow rendering is unchanged. The player receives day/night directional
and ambient lighting; dynamic player-cast terrain shadows are not part of this
renderer.

## Checks

`make test` includes independent pixel rectangle/proportion expectations,
left/right limb separation, pose relationships, displacement-driven animation,
pause/teleport handling, and the existing timing/physics/save tests.

`make test-gl` and native `.\build.cmd -Test` exercise the actual player shaders
and application loop. The fixtures check supplied-skin colors, outer alpha and
depth, texture validation, OpenGL state restoration, first-person rendering,
camera obstruction, view controls, movement poses, and unchanged timed block
removal. Live hand captures compare depth bytes before and after the pass and
keep the central aim region clear at portrait and landscape sizes. Startup
fixtures remove the skin/player shader and substitute a wrong-size skin in a
disposable package. Persistence tests retain their existing save formats and
two-process restart checks.

For interactive review, start a temporary session with `--no-save`, cycle F6,
walk/run/crouch/jump, hold and release breaking, and move the camera near walls.
Repeat with F4, portrait resizing, open chat, capture release, and minimize/restore.
Automated hidden-window captures do not establish how controls feel during play.
