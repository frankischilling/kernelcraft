# Cobblestone texture draft

The approved [source image](../art/cobblestone-source.png) is exported as
[cobblestone.png](../src/assets/textures/cobblestone.png), a 16-by-16, opaque RGBA
tile. It uses the same three colors as `stone.png`: `#6E7071`, `#646464`, and
`#555555`. The larger stone clusters and dark joints distinguish cobblestone
from the existing cracked stone tile.

This is an artwork draft. Cobblestone is not yet a block type or a hotbar item.
The current seven texture-array layers and existing stone artwork are unchanged.
The tile is sized for the renderer's nearest filtering and repeated UVs; the
historical atlas is not involved.

## Source and export

The source was created with the built-in ImageGen tool using the repository's
`stone.png` as its style reference. The selected source is preserved at its
generated resolution. The game-sized export samples each of the 16-by-16 cell
centers, maps each sample to the nearest reference palette color by squared RGB
distance, and writes opaque RGBA pixels. This removes generated gradients and
keeps the output crisp. No source pixels were painted by hand.

The final generation prompt was:

```text
Use case: style-transfer.
Create a cobblestone variant of the reference voxel-game stone PNG. Match its flat, muted gray pixel art. The new tile must read as individual small cobbles packed together, not a cracked continuous rock surface.
One tile, straight-on, edge to edge. Coarse 16x16 pixel sprite, shown enlarged with hard square pixels. About 12 irregular roughly square stones, each 3-5 logical pixels across, a staggered mix of small and medium chunks. Separate every cobble from its neighbors with dark gray joints, 1 logical pixel thick. Light gray flat stone centers, a few mid-gray edge pixels. Short discontinuous joints in varied directions, no long diagonal veins, no repeated diagonal stripes, no large uninterrupted stone patches. Random fitted cobble layout with seamlessly repeating image edges in both axes.
Use exactly 3 opaque colors from the reference: light stone #6E7071, mid gray #646464, joints #555555. These colors are close in brightness: maintain the subdued contrast. No black, no white, no gradients, no grain, no tiny detail within a pixel. No perspective, bevels, cast shadows, scene, frame, border, lettering, or watermark. Return the flat tile only.
```

## Validation

The game-sized tile was inspected as a repeated 4-by-4 panel with nearest
sampling. The repository's `stb_image` decoder was used to check the export's
dimensions, RGBA channels, full opacity, and exact match to the stone palette.
No in-game cobblestone integration or gameplay testing is claimed by this draft.
