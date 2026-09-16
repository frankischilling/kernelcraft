# Player inventory, crafting, and equipment

Press **E** to open the player inventory. The panel has 27 storage slots,
nine hotbar slots, four armor slots, an offhand slot, a 2×2 crafting grid, and
a preview of the supplied player skin. E or Escape closes it. The preview
turns toward the pointer and shows equipped armor; the same equipment is
drawn on the third-person player.

The arrangement and mouse interactions follow the classic Java inventory.
KernelCraft retains its documented **999-item stack limit**, existing hand
breaking rates, and six building materials. Equipment has a stack limit of one.
This does not add Minecraft's complete item catalog, recipe book, combat,
durability, or 3×3 crafting table.

## Controls

| Input | Behavior |
| --- | --- |
| E / Escape | Open or close inventory; Escape outside retains mouse capture controls. |
| Left click | Pick up, place, merge, or exchange a stack. |
| Right click | Pick up the larger half, place one item, or exchange different compatible stacks. |
| Left / right drag | Divide a held stack evenly / place one item in each eligible slot. |
| Shift-click | Transfer storage/hotbar items, equip matching empty armor slots, or craft all whole recipes that fit. |
| Double left click | Gather matching items onto the cursor, up to the stack limit. |
| 1–9 | Swap the hovered slot with a hotbar slot; outside inventory select that slot. |
| F in inventory | Exchange the hovered slot with the offhand. F outside still toggles debug flight. |
| Q / Ctrl-Q | Drop one / the whole stack from the cursor or hovered slot; outside use the selected hotbar slot. |
| Click outside the panel | Drop the cursor stack with left click, or one item with right click. |
| Mouse wheel outside inventory | Cycle the selected hotbar slot. |

A drag commits on release. Visiting a slot twice does not count it twice.
Incompatible armor slots and full stacks do not participate. Splitting ten
items over three empty slots places three in each and leaves one on the cursor.
A capped slot does not redirect its unused share to another slot.

The inventory pauses movement, mouse-look, breaking, dropped-item physics, and
the day/night clock. Gameplay shortcuts and chat entry remain blocked until
it closes. Focus loss, minimization, and framebuffer changes cancel a pending
drag without changing ownership. Window mouse coordinates are converted to
framebuffer pixels before hit testing. The first mouse sample after capture
is discarded.

## Materials and crafting

New worlds, and migrations from saves without an inventory, start with 999 of
each existing material in hotbar slots 1–6: grass, dirt, stone, cobblestone,
oak planks, and stone bricks. The first four storage slots contain a leather
cap, tunic, pants, and boots. Existing v5 inventories are loaded exactly;
their contents are never refilled on restart.

Place one stone in each crafting square to make **four stone bricks**. Each
square can hold a stack. The result is derived from the inputs. Clicking it
consumes one stone from each square only when all four bricks fit on the cursor.
Shift-clicking consumes only complete recipes whose output fits in carried
storage. Equipment cannot enter incompatible armor slots, and recipe results
cannot be overwritten with cursor items.

The leather set uses colored geometry and icons while dedicated armor artwork
remains on the content backlog. It follows the existing body joints; the cap
leaves the face visible. Armor does not alter collision size or movement.
Damage reduction is inactive because health and damage mechanics are still
unimplemented. The existing first-person bare arm remains the foreground
model; separate held-item models remain planned.

## Placement, pickup, and overflow

Successful placement consumes one item. Rejected placement consumes nothing.
When the selected hand has no placeable block, placement can use a block in
the offhand. Right-clicking with selected armor equips it, exchanging an
existing piece when necessary.

Completed hand breaking creates one item of the removed material. Dropped
blocks reuse the existing images as spinning sprites; equipment uses a colored
marker. Gravity and voxel contact run at 120 Hz, with at most eight steps per
frame. Nearby items are collected after a short delay, merging into carried
stacks before using empty slots. Partial capacity collects only what fits.
Drops currently do not expire.

The finite world has room for 128 dropped stacks. Nearby matching drops can
merge up to their normal limit. Dropping and breaking reject an operation when
its new item would not fit; existing items remain intact and the HUD reports
the reason. Items covered by a placed block remain collectible and resume
falling after the obstruction is removed.

Closing returns cursor items and crafting ingredients to carried storage,
then drops any remainder near the player. This is one transaction: when the
drop pool cannot hold the remainder, the panel stays open with the original
items. Free storage or collect existing drops before retrying. Dropping outside
the finite world's valid bounds is also rejected without consuming items.

## Rendering and saves

The panel uses a 176×166 logical layout. Automatic integer scaling preserves
a minimum 320×240 logical canvas; exceptionally small windows fit the panel
fractionally. The preview uses a reusable private framebuffer and depth
attachment, so it cannot erase or obstruct the world depth. Inventory and
equipment remain filled during terrain wireframe and restore caller GL state.

Save version 5 stores all 46 owned stacks, including cursor and crafting
inputs, plus active world drops. The result is recomputed after loading.
Closing the application with pending cursor/crafting items preserves them;
the next launch reopens the inventory. Versions 1–4 still load, while older
executables cannot read v5 saves. See [the exact format](world-persistence.md).

## Regression coverage

`make test` and native `.\build.cmd -Test` include conservation, invalid IDs and
counts, slot compatibility, capped transfers, split/drag examples, recipe
capacity, close overflow, drop physics/contact/pickup, and legacy save loading.
Failed decode and failed replacement preserve live state or the previous save.

The application harness drives production input callbacks for crafting,
equipment, moving, dropping, focus cancellation, full-pool closure, and finite
placement. Live frames cover the equipped preview, cursor output, third-person
armor, portrait resize, and minimize/restore. Separate processes verify pending
inventory ownership on restart. Graphical checks verify layout, preview pixels,
caller GL state, world-depth isolation, and moved material icons against source
PNGs.

For interactive review, launch with `--no-save` or a disposable `--world` path.
Equip the starter armor, split stone across four squares, craft bricks, move
them to the hotbar, and place them. Drop a stack with Ctrl-Q and walk over it.
Resize and minimize with the panel open, then close it and check mouse-look
resumes without a jump. Hidden-window regressions exercise the application
but do not establish physical-input feel or every monitor scaling setup.
