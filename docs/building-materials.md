# Oak planks and stone bricks

Oak planks and stone bricks are implemented on `feat/planks-stone-bricks`,
tracked by issue #42. Press **5** for oak planks or **6** for stone bricks,
then right-click a block face to place one. Hold left mouse on the same block
to break it: oak planks take 1 second and stone bricks take 2 seconds by hand.
The hotbar shows each tile and its selected material name. Slots 7–9 remain
empty and can break blocks without placing anything.

Both materials are opaque, solid cubes with the existing selection outline,
body exclusion, collision, and chunk-seam updates. Each uses the same tile on
all six faces, with fixed face UV orientation and no placement rotation or
random alternate. Greedy quads repeat the tile once per block. They are
available from the hotbar; terrain generation and existing world contents
retain their previous behavior.

| Material | Block ID | Texture layer | Hand break time |
| --- | --- | --- | --- |
| Oak planks | 5 | 8 | 1 second |
| Stone bricks | 6 | 9 | 2 seconds |

The new layers follow cobblestone at layer 7. Existing base layers 0–3 and
terrain variants 4–6 keep their ordering. The runtime files are opaque
16×16 RGBA tiles: [oak planks](../src/assets/textures/oak-planks.png) and
[stone bricks](../src/assets/textures/stone-bricks.png). The refreshed tiles and
their current palettes are editable in [textures.aseprite](../art/textures.aseprite).
Current viewing copies of all runtime PNGs are also in `art/`. See the
[texture source notes](../art/README.md) for export requirements and atlas usage.

## Save compatibility

New writes use version 4 with the existing 72-byte header, payload order,
checksum, and checked replacement. Versions 1 and 2 accept block IDs 0–3;
version 3 also accepts cobblestone ID 4; version 4 accepts IDs 0–6. Older
versions containing the new IDs are rejected before changing the live world.
All nine selected slots still round-trip. Builds supporting only versions
1–3 cannot open a version 4 save, even if no new materials were placed.

## Validation

CPU regressions cover both materials at a negative chunk corner, solid body
collision, DDA targeting, placement rejection, homogeneous and mixed greedy
meshes, hand timing at 30/60/120 FPS, save round-trips, and rejection of new IDs
in legacy versions without changing live state.

The graphical fixtures compare all six faces of each material and three
mixed prisms against independent unit-cube texture draws: 54 views total.
Each new material also has 18,432 shader samples checked for the correct
layer without variants. HUD checks compare all six icons against the PNGs,
check opacity and names, and verify cleanup when any building icon fails to
load. Application harnesses select, place, and break both new materials in
flight and walking mode, then exercise F5, normal-exit saving, and restart
using temporary world files.

The following commands passed with exit status 0 on Ubuntu/WSL with GCC,
Clang, Mesa, and Xvfb:

```sh
make -j4
make test
make test-sanitize
make test-gl
make test-build
make CC=clang CFLAGS='-O2 -g -Werror' all test test-gl
```

Native Windows checks passed with MinGW64 and Intel UHD graphics:

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
```

All 54 face comparisons matched on both platforms. Native HUD captures show
the new icons and selected names. Read-only code review found no remaining
actionable issues. Physical interactive playtesting remains unverified. Logs, trees, other wood
families, and masonry variants remain separate roadmap work.
