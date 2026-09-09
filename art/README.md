# Block texture artwork

The current editable block textures are in [textures.aseprite](textures.aseprite).
[texture_atlas.aseprite](texture_atlas.aseprite) contains the atlas overview.
This folder also contains the current PNG exports for easy viewing. Their
filenames and contents match `src/assets/textures/`, including the reference
atlas. The three obsolete `*-source.png` images have been removed.

The game loads the ten individual PNGs in `src/assets/textures/`. Keep these
exports at 16 by 16 pixels, fully opaque RGBA, with their existing filenames.
Keep the viewing copies here synchronized with those runtime exports.
The renderer repeats them with nearest sampling; the hotbar uses the same
material images. Keep the grass top, side, and terrain variants separate.

`src/assets/textures/texture_atlas.png` is a reference sheet. The game does not
load it or use `atlast.py`; runtime materials use independent texture-array
layers. Updating the sheet does not change layer ordering or UV coordinates.
