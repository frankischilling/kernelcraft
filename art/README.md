# Block texture artwork

The current editable block textures are in [textures.aseprite](textures.aseprite).
[texture_atlas.aseprite](texture_atlas.aseprite) contains the atlas overview.
The earlier `cobblestone-source.png`, `oak-planks-source.png`, and
`stone-bricks-source.png` files are retained as previous artwork.

The game loads the ten individual PNGs in `src/assets/textures/`. Keep these
exports at 16 by 16 pixels, fully opaque RGBA, with their existing filenames.
The renderer repeats them with nearest sampling; the hotbar uses the same
material images. Keep the grass top, side, and terrain variants separate.

`src/assets/textures/texture_atlas.png` is a reference sheet. The game does not
load it or use `atlast.py`; runtime materials use independent texture-array
layers. Updating the sheet does not change layer ordering or UV coordinates.
