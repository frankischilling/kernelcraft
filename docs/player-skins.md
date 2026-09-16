# Player skins and camera views

The player uses the supplied `art/skin.png` through its identical runtime copy
at `src/assets/player/skin.png`. The six cuboids have separate base and optional
outer layers. The [artwork layout](../art/README.md#player-skin) specifies every
face rectangle of the 64×64 RGBA sheet, including the distinct left limbs.

F6 cycles first-person, rear third-person, and front third-person views. A third-person
camera normally sits three blocks from the eye. Center, edge, and corner probes
limit its distance near terrain. When less than 0.8 blocks remain, that frame
uses first-person rendering. This does not change the selected camera mode.
The selected third-person camera keeps the standing-height anchor while crouched;
only the first-person eye moves down to the crouched eye height.
F6 ignores repeats, chat or inventory entry, inactive/minimized windows, and zero framebuffer
sizes. The chosen mode survives pauses and resets to first person on restart.

The original eye camera still controls movement, aiming, block selection,
placement, breaking, and the saved yaw/pitch. Third-person front view is an
inspection view: editing still follows the player's aim, away from that display
camera. The model and display camera do not change collision dimensions, standing
or crouched eye heights, reach, block-breaking durations, or save formats.

E opens the [inventory](inventory.md), which renders the same skin into a private
preview framebuffer. The preview follows the pointer without turning the world
camera. Equipped leather pieces follow the same body joints in that preview
and in third person. They use code-colored geometry while dedicated artwork is
pending; the cap leaves the face visible. Equipment is stored in v5 saves and
does not change collision dimensions. First-person held-item and armor sleeve
models remain future work.

## Model and animation

`world/player_model.c` holds the CPU-only skin rectangles, dimensions, joints,
and poses. Local X points to the wearer's right, Y points up, and the front
faces negative Z. The standing model is 32 skin pixels high, scaled to the
existing 1.8-block player height. Arms and legs are four pixels wide; the torso
is eight pixels wide. The classic wide-arm joints place the shoulders five
pixels from the center and 22 pixels above the feet. The torso pivots at the
neck, while the hip pivots are 1.9 pixels from the center. The head follows the
viewing pitch. The bottom UV row direction follows the folded skin net, including
the palm and sole faces.

Walking phase advances from actual horizontal displacement on credited physics
ticks, at `4 * 0.6662` radians per block. Swing weight follows actual speed;
the legs swing 1.4 times as far as the opposite arms. Running increases the swing;
standing against a wall stops phase advance and eases the limbs toward rest.
Airborne movement has a separate pose. Crouching keeps the same full-size body
parts and uses joint rotations/translations for the bent posture, so entering
third person does not make the character smaller. The posed outer shell is
aligned to the feet so rotated legs do not sink into the floor. Physics retains
its existing one-block crouch collision and immediate posture changes; in a
one-block passage the full-size visual model can intersect the surrounding blocks.

Punches use a repeating 0.3-second visual cycle sampled from the existing
`BlockBreaking.elapsed` value. They do not accumulate another gameplay timer.
Progress travels from zero to one, with a forward strike and a lowered recovery
that returns to rest. The third-person strike also turns the torso and shoulders.
Resetting/cancelling the break returns the hand to rest, including release,
changed slots, right-click, flight transitions, pause, and target/reach changes.
Held breaking still removes a block only when the original material duration
has elapsed. Pauses clear gait and punch state without a resumed-frame jump.

The first-person arm has its own transform, using the classic bare right-hand
pose and swing sequence with a neutral equip offset. The shared four-pixel wrist
uses sixteenth-block model units, with the shoulder extending below the screen.
It does not inherit world-space shoulder rotations, crouch translation, or root
scale. Walking contributes a small foreground bob. The normal 16:9 composition
follows the classic transform; narrower windows fit the arm uniformly.

The reference conventions come from the classic `ModelBox` skin net,
`PlayerModel` wide-arm joints, and `ItemInHandRenderer.renderPlayerArm`. The
[published bare-arm transform](https://forums.minecraftforge.net/topic/121014-1193-displaying-hands-in-first-person/)
uses a monotonic swing parameter; the CPU tests check independently calculated
hand positions along that path. KernelCraft retains its existing movement,
one-block crouch height, and material-breaking rules.

## Rendering

`graphics/player_renderer.c` owns one static vertex buffer and one RGBA8
`GL_TEXTURE_2D`. Skin sampling uses nearest filtering, clamp-to-edge, and no
mipmaps. The source must decode as exactly 64×64 with four channels. Missing,
incorrectly sized, or invalid assets fail initialization and release partial GPU
resources while the context remains current.

The body draws after terrain/sky and before selection/clouds with ordinary
depth testing. Outer geometry grows in all three axes: half a skin pixel per
face for the head and a quarter pixel per face for the body and limbs. Base and
outer caps therefore have separate depth planes. Where different parts overlap
at the shoulders, jacket hem, or legs, a small fixed polygon offset gives each
part a stable depth order. The offset has no slope factor, so it does not grow
as the camera turns. Rendering restores the caller's offset settings afterward.
Fully transparent skin fragments discard. Opaque outer-layer fragments write
depth; fractional outer alpha blends without changing depth.
When the selected main-hand slot is empty, the first-person pass draws the same
right-arm mesh and sleeve in camera space
after clouds and before the HUD. It preserves the world depth buffer so nearby
terrain cannot cut through the hand and the HUD remains independent.
The resting arm leaves aim clear. The strike sweeps inward toward the target,
with the crosshair and breaking bar drawn over it by the HUD.

Nonempty hands use the shared [3D item models](inventory.md#rendering-and-saves).
First-person items have an independent depth attachment and are composited over
the world. World and inventory-preview items follow the corresponding arm's
joint transforms, including the root pose. The inventory portrait uses neutral
standing proportions, bounded mouse look, and studio lighting so its head and
feet remain framed beside the armor slots while world simulation continues.

Both player passes draw filled geometry even when F4 makes the terrain wireframe,
then restore the OpenGL state they changed. The skin uses its own texture/shader
path; it does not add layers or materials to the terrain array. Existing cached
terrain shadow rendering is unchanged. The player receives day/night directional
and ambient lighting; dynamic player-cast terrain shadows are not part of this
renderer.

## Checks

`make test` includes independent pixel rectangle/proportion expectations,
left/right limb separation, physical hand/foot swing direction, classic
first-person hand-position goldens, full crouched shell bounds,
displacement-driven animation, pause/teleport handling, and the existing
timing/physics/save tests.

`make test-gl` and native `.\build.cmd -Test` exercise the actual player shaders
and application loop. The fixtures check supplied-skin colors, outer alpha and
depth on all six faces from multiple angles, 468 camera-orbit samples of painted
base/outer joint seams, asymmetric bottom-face UV markers, texture validation,
OpenGL state restoration, first-person wrist size and return to rest,
camera obstruction, view controls, movement poses, and unchanged timed block
removal. Live hand captures compare depth bytes before and after the pass and
check the resting aim region at portrait and landscape sizes. Startup
fixtures remove the skin/player shader and substitute a wrong-size skin in a
disposable package. Persistence tests retain legacy v1–v4 compatibility and
exercise v5 inventory/equipment state through two-process restart checks.

For interactive review, start a temporary session with `--no-save`, cycle F6,
walk/run/crouch/jump, hold and release breaking, and move the camera near walls.
Repeat with F4, portrait resizing, open chat, capture release, and minimize/restore.
Automated hidden-window captures do not establish how controls feel during play.
