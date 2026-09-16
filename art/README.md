# Artwork

The current editable block textures are in [textures.aseprite](textures.aseprite).
[texture_atlas.aseprite](texture_atlas.aseprite) contains the atlas overview.
This folder also contains a PNG copy of every current texture for easy viewing.
The PNGs have the same filenames and contents as `src/assets/textures/`.

The game loads thirteen individual PNGs in `src/assets/textures/`. Keep these
exports at 16 by 16 pixels, fully opaque RGBA, with their existing filenames.
The renderer repeats them with nearest sampling; the hotbar uses the same
material images. Keep the grass top, side, and terrain variants separate.
After exporting a texture, update both its runtime PNG and the matching PNG
here so the viewable artwork stays current.

The supplied oak exports are `oak-log-side.png`, `oak-log-top.png`, and
`oak-leaves.png`. Logs use bark on four sides and end grain on both ends;
leaves use their opaque tile on every face. Leafy ground uses the existing
`grass-top-leaves.png`. These exports do not require changes to the atlas or
editable source files to load in game.

`src/assets/textures/texture_atlas.png` is a reference sheet. The game does not
load it or use `atlast.py`; runtime materials use independent texture-array
layers. Updating the sheet does not change layer ordering or UV coordinates.

## Sky and celestial bodies

`celestial.aseprite` is the editable celestial artwork. The 64-by-64 exports
`day.png`, `dawn-dusk.png`, and `night.png` supply the sky palettes; `sun.png`
and the eight moon exports supply the visible bodies. The moon images are
`full-moon.png`, `waning-gibbous.png`, `last-quarter.png`, `waning-crescent.png`,
`new-moon.png`, `waxing-crescent.png`, `first-quarter.png`, and `waxing-gibbous.png`.
Their runtime copies live in
`src/assets/sky/`. Keep each export and its runtime copy identical.

The sky samples the five palette colors and smoothly interpolates between
them. Day and night share 24-degree band spacing and smooth blends. Day uses
top-to-bottom rows at -30, -6, 18, 42, and 66 degrees, one band lower than
night, so its pale horizon strip is narrower and cyan/blue cover more sky.
Night reverses its PNG row order at -6, 18, 42, 66, and 90 degrees, keeping
dark purple overhead. Dawn/dusk uses top-to-bottom rows at night's heights,
placing orange just below the horizon. The supplied colors and PNGs remain
unchanged.
The three palettes blend as time advances.
The sun and all moon phases retain their square outlines and nearest sampling.
The sky shader draws pixel-stepped square halos behind both bodies, tinted
yellow-orange for the sun and white for the moon, with softer scattering and
bloom around them. The supplied images do not need glow painted into them.
Moon glow and direct moonlight scale with the phase. The new moon keeps its
supplied dark surface and emits no glow or direct light. These are discrete
daily sprites; astronomical lunar geometry remains in the README TODO list.

## Player skin

`skin.png` is the player skin. Keep it byte-identical to
`src/assets/player/skin.png`, which both builds package at `assets/player/skin.png`.
Export exactly 64 by 64 pixels with four RGBA channels. The classic arm width is
four pixels. Left and right arms and legs have independent regions; a 64-by-32
legacy skin or three-pixel slim-arm layout is not supported.

Coordinates start at the PNG's top-left: X increases right and Y increases down.
The runtime uses these coordinates directly, without flipping or resizing the
image. The skin has its own nearest-filtered 2D texture and no mipmaps. It is
independent of the terrain texture array and its layer numbers.

Each cuboid unwrap begins at `(u, v)`, using pixel dimensions `(w, h, d)`:

| Face | Rectangle `(x, y, width, height)` |
| --- | --- |
| Right | `(u, v + d, d, h)` |
| Left | `(u + d + w, v + d, d, h)` |
| Top | `(u + d, v, w, d)` |
| Bottom | `(u + d + w, v, w, d)` |
| Front | `(u + d, v + d, w, h)` |
| Back | `(u + 2d + w, v + d, w, h)` |

A rectangle covers X through X + width - 1 and Y through Y + height - 1.
Right and left refer to the wearer's sides. On each vertical face, the first row
is the top; the first column is the left edge when looking directly at that face
from outside. On both the top and bottom faces, the first row touches the back
and the first column touches the wearer's right. The bottom face reverses V
when the net folds around the cuboid; the PNG itself is never flipped.

| Body part | Dimensions `(w, h, d)` | Base origin `(u, v)` | Outer origin `(u, v)` |
| --- | --- | --- | --- |
| Head | `(8, 8, 8)` | `(0, 0)` | `(32, 0)` |
| Torso | `(8, 12, 4)` | `(16, 16)` | `(16, 32)` |
| Right arm | `(4, 12, 4)` | `(40, 16)` | `(40, 32)` |
| Left arm | `(4, 12, 4)` | `(32, 48)` | `(48, 48)` |
| Right leg | `(4, 12, 4)` | `(0, 16)` | `(0, 32)` |
| Left leg | `(4, 12, 4)` | `(16, 48)` | `(0, 48)` |

Keep base face pixels opaque. Leave unused cells transparent. Outer layers may
be completely transparent, partially painted, or translucent. Transparent
outer pixels reveal the base and do not write depth. The head shell extends
half a skin pixel beyond every face; body and limb shells extend a quarter
pixel on every face, including the top and bottom. The first-person hand reuses
the right arm and sleeve regions. See [player rendering](../docs/player-skins.md)
for poses and checks.
