# Player inventory, crafting, and equipment

Press **E** to open the player inventory. The panel has 27 storage slots,
nine hotbar slots, four armor slots, an offhand slot, a 2×2 crafting grid, and
a preview of the supplied player skin. E or Escape closes it. The preview
turns toward the pointer and shows equipped armor and held items. Its standing
body fits between the helmet and boot slot edges. The same equipment and held
items appear on the third-person player.

The arrangement and mouse interactions follow the classic Java inventory.
KernelCraft retains its documented **999-item stack limit**, existing hand
breaking rates, and eight placeable block items. Equipment has a stack limit of one.
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

Opening inventory blocks movement commands, mouse-look, breaking, placement,
and chat entry. The world keeps running: gravity, collision, the day/night
clock, clouds, and dropped-item physics/pickup continue. A falling player still
lands while the panel is open. Picking up items during a drag changes carried
storage without duplicating or replacing the cursor stack.

Chat, ordinary released mouse capture, focus loss, minimization, and zero-size
framebuffers retain their pause behavior. Focus loss and framebuffer changes
cancel pending drags without changing ownership. Window mouse coordinates are
converted to framebuffer pixels before hit testing. The first mouse sample
after capture is discarded.

## Materials and crafting

New worlds, and migrations from saves without an inventory, start with 999 of
each existing material in hotbar slots 1–6: grass, dirt, stone, cobblestone,
oak planks, and stone bricks. The first four storage slots contain a leather
cap, tunic, pants, and boots. Logs are obtained from oak trees; leaves drop
nothing when broken. Existing saved leaf items remain valid and placeable.
Existing v5/v6 inventories are loaded exactly;
their contents are never refilled on restart.

Place one stone in each crafting square to make **four stone bricks**. Each
square can hold a stack. The result is derived from the inputs. Clicking it
consumes one stone from each square only when all four bricks fit on the cursor.
Shift-clicking consumes only complete recipes whose output fits in carried
storage. Equipment cannot enter incompatible armor slots, and recipe results
cannot be overwritten with cursor items.

One oak log in any single crafting square, with the other three empty, makes
**four oak planks**. Clicking consumes one log only when all four planks fit.
Shift-clicking repeats complete recipes that fit, using the same 999-item limit.

The leather set uses colored geometry and icons while dedicated armor artwork
remains on the content backlog. It follows the existing body joints; the cap
leaves the face visible. Armor does not alter collision size or movement.
Damage reduction is inactive because health and damage mechanics are still
unimplemented. Selecting a block or equipment item displays its 3D model in the
right hand with its skinned arm and sleeve; an occupied offhand draws the left
arm with that hand's separate skin regions. An empty main hand displays
the supplied skin's bare arm. Held items follow walking and the existing punch
cycle without changing block-breaking times. Breaking uses distinct strike and
recovery poses instead of replaying the same held-block path backward.

## Placement, pickup, and overflow

Successful placement consumes one item. Rejected placement consumes nothing.
When the selected hand has no placeable block, placement can use a block in
the offhand. Right-clicking with selected armor equips it, exchanging an
existing piece when necessary. A committed placement starts a short swing in
the hand that supplied the block. The renderer keeps the consumed item visible
through the swing when placement used the last item in that stack. Rejected
placements do not start the animation.

Completed hand breaking creates one item of the removed material except leaves,
which drop nothing. Leafy grass gives an ordinary grass block, and logs drop
their own item. Leaf removal never requires free dropped-item storage. Dropped
blocks use textured 3D cubes; equipment uses shaped colored models with visible
thickness. Gravity and voxel contact run at 120 Hz, with at most eight steps per
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

The preview uses a neutral standing pose and fixed studio lighting. Crouching,
falling, and changes in world light do not shrink or darken it. Its orthographic
view preserves body proportions while the pointer turns the body and head.

`world/item_model.c` defines bounded CPU geometry for the eight block items and
four leather pieces. Block faces use the existing terrain base images, including
grass top, side, and dirt underside. Position-dependent terrain variants remain
cosmetic and do not create extra item types. `graphics/item_renderer.c` uploads
these meshes once, then shares them between world and hand draws. It renders
transparent 128×128 inventory icons once at initialization; those cached views
use nearest sampling in the hotbar, slots, crafting result, and cursor.
Equipment models use simple colored shells while dedicated artwork remains
pending. First-person items use a reusable private color/depth target and a
transparent composite, preserving world depth even beside a wall.

Save version 6 stores all 46 owned stacks, including cursor and crafting
inputs, plus active world drops. The result is recomputed after loading.
Closing the application with pending cursor/crafting items preserves them;
the next launch reopens the inventory. Versions 1–5 still load. Version 6 adds
log/leaf item IDs without renumbering leather equipment; older executables
cannot read v6 saves. See [the exact format](world-persistence.md).

## Regression coverage

`make test` and native `.\build.cmd -Test` include conservation, invalid IDs and
counts, slot compatibility, capped transfers, split/drag examples, recipe
capacity, close overflow, drop physics/contact/pickup, and legacy save loading.
Failed decode and failed replacement preserve live state or the previous save.

The application harness drives production input callbacks for crafting,
equipment, moving, dropping, focus cancellation, full-pool closure, and finite
placement. Live frames cover the equipped preview, cursor output, third-person
armor, held blocks/equipment, empty-hand fallback, portrait resize, and
minimize/restore. Separate processes verify pending inventory ownership on
restart. Geometry tests check outward faces, UV coordinates, volume, equipment
openings, and stable texture-layer mappings. Graphical checks compare all six
faces of all eight block items against the source PNGs, and check icon placement,
preview height, caller GL state, world-depth isolation, allocation failure
recovery, held-item visibility, distinct breaking strike/recovery silhouettes,
and main/offhand placement motion. Input checks verify placement animation starts
only after a committed edit and preserves a consumed last item for the swing.

For interactive review, launch with `--no-save` or a disposable `--world` path.
Equip the starter armor, split stone across four squares, craft bricks, move
them to the hotbar, and place them. Drop a stack with Ctrl-Q and walk over it.
Resize and minimize with the panel open, then close it and check mouse-look
resumes without a jump. Hidden-window regressions exercise the application
but do not establish physical-input feel or every monitor scaling setup.
