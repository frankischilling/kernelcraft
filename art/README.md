# Artwork

The current editable block textures are in [textures.aseprite](textures.aseprite).
[texture_atlas.aseprite](texture_atlas.aseprite) contains the atlas overview.
This folder also contains a PNG copy of every current texture for easy viewing.
The PNGs have the same filenames and contents as `src/assets/textures/`.

The game loads the ten individual PNGs in `src/assets/textures/`. Keep these
exports at 16 by 16 pixels, fully opaque RGBA, with their existing filenames.
The renderer repeats them with nearest sampling; the hotbar uses the same
material images. Keep the grass top, side, and terrain variants separate.
After exporting a texture, update both its runtime PNG and the matching PNG
here so the viewable artwork stays current.

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
