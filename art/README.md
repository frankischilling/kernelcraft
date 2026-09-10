# Block texture artwork

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
