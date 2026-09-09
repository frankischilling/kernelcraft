# Cobblestone block and texture

This records the cobblestone milestone merged in PR #37. For the current
hotbar and save version, see [building materials](building-materials.md).

The approved [source image](../art/cobblestone-source.png) is exported as
[cobblestone.png](../src/assets/textures/cobblestone.png), a 16-by-16, opaque RGBA
tile. It uses the same three colors as `stone.png`: `#6E7071`, `#646464`, and
`#555555`. The larger stone clusters and dark joints distinguish cobblestone
from the existing cracked stone tile.

Press **4** to select cobblestone, then right-click a block face to place it.
Left-click breaks it using the existing block-editing controls. The fourth
hotbar slot shows its flat texture icon and the name "Cobblestone"; slots 5–9
remain empty. These were the slot assignments at that milestone.

Cobblestone is solid block ID 4. Every face uses texture-array layer 7, appended
after the existing seven layers. Greedy meshes merge adjacent cobblestone faces
and repeat the tile once per block, with no random terrain variant. Existing
stone artwork, generated terrain, and layers 0–6 retain their previous behavior.

New saves use version 3 with the same header layout, checksum, and payload order.
Versions 1 and 2 still load; their payloads may contain only the original IDs
0–3. Version 3 also accepts cobblestone ID 4. Old builds reject version 3 saves,
so reopening a newly saved world requires this updated build. Player state and
all nine selected slots round-trip through the existing persistence path.

## Validation

The game-sized tile was inspected as a repeated 4-by-4 panel with nearest
sampling. The repository's `stb_image` decoder was used to check the export's
dimensions, RGBA channels, full opacity, and exact match to the stone palette.
CPU checks cover slot mapping, solid edits at a negative chunk corner, greedy
meshes and mixed material boundaries, save/reload, and legacy-version rejection
of the new ID. The graphical tests compare all six cobblestone faces with an
independent 2D texture reference and verify 18,432 shader samples use layer 7
without an alternate. HUD checks cover the fourth icon, label, and cleanup
after a failed icon load. The actual application harness selects, places,
breaks, saves, and reloads cobblestone using temporary worlds.

The following checks passed on Ubuntu/WSL (GCC, Clang, and Mesa) and native
Windows (MinGW64 and Intel graphics):

```sh
make -j4 all test test-gl
make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl
make CC=clang test-sanitize
make test-build
```

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File tests\test_build.ps1
```

All 36 independent terrain face comparisons matched, and the HUD capture shows
the fourth slot selected with its cobblestone icon and label. Read-only code
review found no actionable issues. Physical interactive playtesting remains
unverified.
