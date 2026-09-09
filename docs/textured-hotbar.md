# Textured nine-slot hotbar

Base: `5fbe94db06a57bd49483d326f0a7cc3eef2c54bd`, after PR #31 was merged.
Branch: `feat/textured-nine-slot-hotbar`.
Tracked by [issue #32](https://github.com/frankischilling/kernelcraft/issues/32).

The hotbar has nine numbered slots. Grass, dirt, and stone occupy slots 1–3;
slots 4–9 are empty. The three items use the existing `grass-side.png`,
`dirt.png`, and `stone.png` artwork as upright, flat sprites with nearest
filtering. Terrain variants remain cosmetic and do not become separate items.
The selected slot has a gold border and an item name above the bar when space
allows. Keys 1–9 select slots while the mouse is captured; repeats, released
capture, focus loss, and minimized windows retain their existing input gates.
An empty slot can break blocks but cannot place them.

The bar fits both framebuffer width and height, keeping the crosshair clear
even in wide, short windows. Icons are 32 pixels at ordinary sizes, 16 when
space is limited, and smaller at the minimum layout. Below 192 pixels wide or
120 high, the bar is hidden. This is a fixed hotbar, with no inventory storage,
stack counts, dragging, or crafting interface.

Three icon textures are loaded during HUD initialization and reused each frame.
Failed initialization releases partial resources. Cleanup deletes the icons
before the GL context is destroyed. HUD rendering restores the texture unit,
bindings, texture environment, blend functions/equation, polygon mode, culling,
and the existing text/matrix/depth/line/color state.

## Save compatibility

The selected slot is independent of its block contents. Version 2 saves store
the one-based slot number 1–9 at header offset 60; all other header fields,
payload bytes, checksums, and replacement rules retain their meaning.
Version 1 saves still load, mapping their block IDs 1–3 to the same numbered
slots. Invalid slot values are rejected before live world state changes.
New saves use version 2 and cannot be reopened by older builds. The user's
existing world is not rewritten until an ordinary save in the updated game.

## Verification

The initial HUD regression failed on missing slot numbers 4–9, and the actual
application fixture failed when key 4 retained a solid block selection.
Both pass with the new implementation. A review found that the initial layout
covered the crosshair at 640×120 and 1280×120; added pixel cases reproduced it
and pass after limiting the bar's height. The follow-up read-only review found
no remaining actionable issue; that reviewer did not run tests.

HUD checks cover compact/debug modes, all nine selected borders, landscape,
portrait, wide/short, tiny, and zero-sized framebuffers. Complete icon pixels
are compared against independently decoded PNGs, checking orientation, size,
and filtering. Texture handles stay constant across frames and are invalid
after cleanup; initialization and drawing preserve the tested GL state.

CPU checks round-trip all nine slots, load each version 1 material, reject
invalid version/slot fields, and retain existing corruption and replacement
failure coverage. The application selects empty slots through registered key
callbacks, rejects placement, and still breaks blocks. The restart fixture
checks an F5 save with stone selected, switches to slot 9, exits, and verifies
the empty selection and edited world in a second process.

Validation from the repository on 2026-09-08:

| Environment | Command | Result |
| --- | --- | --- |
| WSL Ubuntu, GCC | `make -j4 all test test-gl` | Passed |
| WSL Ubuntu, GCC | `make test-sanitize` | CPU ASan/UBSan passed |
| WSL Ubuntu, Clang | `make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test` | Build and CPU checks passed |
| Native Windows | `.\build.cmd -Test` | Release CPU/application/GL checks passed |
| Native Windows | `.\build.cmd -Configuration Debug -Test` | Debug CPU/application/GL checks passed |
| WSL Ubuntu, Mesa GL 3.3 | `MESA_GL_VERSION_OVERRIDE=3.3COMPAT MESA_GLSL_VERSION_OVERRIDE=330 xvfb-run -a ./test-hud` from `bin/linux/Release` | Passed |
| Native Windows | `.\bin\windows\Release\test-startup.exe --no-save` with a temporary capture prefix | Application checks passed; captures inspected |

Linux used GCC 13.3.0, Clang 18.1.3, GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0, and Mesa
llvmpipe. Windows used MinGW64 GCC 13.2.0, GLFW 3.3.8, GLEW 2.2.0,
freeglut 3.4.0, and Intel UHD Graphics. HUD captures were visually inspected.
Logs and captures use `%TEMP%/kernelcraft-hotbar-*`. The existing default save
retained SHA-256 `496aa5253c74f0338e518cb1055276cf2c366ab839376ef6910a079bb04d6735`
through validation. Tests use temporary worlds or `--no-save`.

The requested Humanizer, Git Commit Author, and Git Human Workflow manifests,
helper path, and configured Francis Hagan identity are listed in the
[workflow record](windows-incremental-build.md#workflow-and-next-milestone).
They were used for this increment, with Git Human Workflow owning all Git and
GitHub commands. No global identity changes, new hooks, or history rewrites
were needed.

Build and run with `.\build.cmd -Run` or `make run`. Exercise keys 1–9, place
and break with the first three slots, then verify empty-slot behavior and
selection after restart in a disposable world. Physical keyboard/mouse
playtesting and monitor scaling remain unverified. Foundation acceptance is
still tracked by [issue #9](https://github.com/frankischilling/kernelcraft/issues/9);
inventory, trees, caves, streaming, and later gameplay remain planned.
