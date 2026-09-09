# Block, building, and item design backlog

Everything below is planned. These are content and design tasks, not a list of
items available in the current build. Grass, dirt, and stone are the implemented
block materials. Keep the existing voxel foundation and develop the colorful,
explorable Cube World-inspired direction through coherent content sets.

## Terrain and underground blocks

- [ ] Bedrock: a dark, fractured base layer with explicit unbreakable behavior.
- [ ] Sand and red sand: pale coast/desert grains and a warmer canyon variant.
- [ ] Gravel: readable mixed pebbles; decide falling-block behavior separately.
- [ ] Clay and mud: smooth riverbank clay and darker wet ground.
- [ ] Packed mud and dried clay: building materials with cracked surface patterns.
- [ ] Snow layers and snow blocks: define thin-layer collision and accumulation.
- [ ] Ice and packed ice: blue-white textures, transparency, and slipperiness rules.
- [ ] Cobblestone and mossy cobblestone: broken rock faces with restrained moss coverage.
- [ ] Granite, limestone, slate, basalt, marble, and sandstone: distinct palettes and grain sizes.
- [ ] Polished stone variants: smoother surfaces that still repeat cleanly across greedy quads.
- [ ] Obsidian and volcanic rock: dark glassy fractures and cooling lava patterns.
- [ ] Coal, copper, iron, tin, gold, and silver ores: deposits readable against their host rock.
- [ ] Crystal, ruby, sapphire, emerald, and diamond ores: distinct shapes as well as colors.
- [ ] Raw ore and refined metal storage blocks, with recipes for packing/unpacking resources.
- [ ] Crystal clusters and geodes: cave decorations with defined selection and collision shapes.

## Trees and plants

- [ ] Oak, birch, pine, willow, and acacia tree sets with recognizable silhouettes.
- [ ] Logs with bark sides, end grain, and placement orientation.
- [ ] Stripped logs, planks, and bark blocks for every supported wood family.
- [ ] Leaves with defined cutout rendering, transparency sorting needs, and decay rules.
- [ ] Saplings, roots, branches, and stumps, including growth and harvest behavior.
- [ ] Tall grass, ferns, reeds, cattails, and dry shrubs.
- [ ] Flowers in several shapes and colors; include garden planting and dye uses.
- [ ] Small mushrooms, large mushroom caps/stems, and cave fungi.
- [ ] Vines, hanging roots, moss carpets, and climbing plants.
- [ ] Cactus and thorn bushes with explicit collision and damage rules.
- [ ] Wheat, carrots, potatoes, berries, pumpkins, and other farm crops with growth stages.

## Building sets and shapes

- [ ] Clay brick, stone brick, mossy brick, and cracked brick sets.
- [ ] Carved sandstone, patterned terracotta, and glazed tile sets.
- [ ] Copper, iron, and brass panels, grates, rivets, and trim.
- [ ] Clear and colored glass, plus window panes and framed windows.
- [ ] Dyed cloth/wool blocks and carpets with a consistent color palette.
- [ ] Slabs, stairs, ramps, and corner roof pieces; define orientation and partial-block collision.
- [ ] Fences, gates, low walls, railings, and posts with neighbor connections.
- [ ] Doors, trapdoors, shutters, and hatches with open/closed collision and persistence.
- [ ] Ladders, ropes, and scaffolding with deliberate climbing and support rules.
- [ ] Wooden beams, stone pillars, arches, and decorative columns.
- [ ] Roof shingles, thatch, slate roofing, and metal roofing.
- [ ] Roads and paths: packed dirt, gravel, cobble, paving tiles, and wooden boardwalks.
- [ ] Signs, banners, flags, and wall plaques with readable placement rules.

## Decorations and functional blocks

- [ ] Torches, candles, lanterns, braziers, and campfires; define light and fire behavior.
- [ ] Glowing crystals and Fire Bug lanterns with emissive textures and a lighting dependency.
- [ ] Tables, chairs, stools, benches, shelves, beds, and cupboards.
- [ ] Flowerpots, planters, baskets, crates, sacks, and barrels.
- [ ] Chests and larger storage containers with inventory and save-format support.
- [ ] Crafting benches, furnaces, kilns, forges, anvils, and workbenches for specialized recipes.
- [ ] Cooking pots, ovens, and drying racks for food preparation.
- [ ] Grindstones and repair stations with explicit durability/material costs.
- [ ] Item frames, weapon racks, armor stands, and display cases.
- [ ] Bookshelves, maps, paintings, rugs, and hanging decorations.
- [ ] Wells, fountains, troughs, and water channels after fluid rendering/storage exists.
- [ ] Switches, pressure plates, levers, and mechanisms as a later coherent interaction system.
- [ ] Cupid Sponge: define its appearance, collection method, and special interaction before implementation.

## Tools and gathering equipment

- [ ] Pickaxes for stone and ores: a narrow handle and distinct pointed mining head.
- [ ] Axes for logs and wooden blocks: a broad cutting head with a readable edge.
- [ ] Shovels for dirt, sand, gravel, and snow: a clear scoop silhouette.
- [ ] Hoes for soil preparation: define farmland, moisture, and crop interaction together.
- [ ] Shears for leaves, plants, and future animal resources.
- [ ] Hammers and chisels for building variants, repair, or shaping; choose their roles before adding overlap with other tools.
- [ ] Fishing rods, hooks, bait, and nets after fishing targets and water behavior exist.
- [ ] Buckets, watering cans, and bottles with explicit fluid/container rules.
- [ ] Torches, portable lanterns, compasses, maps, and binoculars for exploration.
- [ ] Material tiers: wood, stone, copper/bronze, iron, steel, and rare crystal equipment.
- [ ] Define mining speed, harvest requirements, durability, repair costs, and recipes per tool/tier.
- [ ] Distinguish tiers by head shape and detail as well as color; keep grips visually consistent.

## Weapons, armor, and combat items

- [ ] Short swords, longswords, and greatswords with distinct reach and attack timing.
- [ ] Daggers, spears, maces, and war hammers with clearly different combat roles.
- [ ] Bows, crossbows, arrows, bolts, and quivers.
- [ ] Wooden and metal shields with visible blocking feedback and durability.
- [ ] Cloth, leather, chain, plate, and crystal armor sets: helmet, chest, legs, and boots.
- [ ] Gloves, belts, capes, and backpacks; decide cosmetic versus gameplay effects.
- [ ] Staffs and wands for a later magic increment with explicit resource and targeting rules.
- [ ] Define damage, cooldowns, stamina/resource costs, hit detection, and enemy reactions before balancing equipment tiers.
- [ ] Design Goblin equipment and other creature-themed loot without replacing the basic gathering progression.

## Resources, food, and supplies

- [ ] Sticks, fibers, string, rope, leather, cloth, feathers, bones, and resin.
- [ ] Ore chunks, ingots, metal plates, nails, gears, and cut gems.
- [ ] Seeds, grain, flour, fruit, vegetables, mushrooms, and harvested herbs.
- [ ] Bread, cooked meat/fish, soups, pies, and travel rations.
- [ ] Bandages, salves, healing potions, and antidotes after health/status systems exist.
- [ ] Dyes, pigments, paint, and pattern ingredients for decorative variants.
- [ ] Keys, coins, treasure maps, and quest objects once their consuming systems exist.

## Art and delivery tasks for each content set

- [ ] Draw a small palette and silhouette sheet before producing a large family of assets.
- [ ] Keep terrain tiles at the existing 16-by-16 size unless a texture-system change includes a new size and compatibility policy.
- [ ] Design seamless faces, end grain/orientation, top/side/underside mapping, and occasional variants.
- [ ] Create flat inventory icons that remain readable in the existing small hotbar slots.
- [ ] Design held first-person models, third-person attachments, and dropped-item appearances together.
- [ ] Add swing, mining, use, equip, and break animations as their gameplay systems become available.
- [ ] Use original or compatible licensed assets and retain attribution; do not copy another game's textures.
- [ ] Assign stable IDs and define old-save compatibility before adding blocks or item metadata.
- [ ] Connect each implemented block through materials, editing, selection, collision, meshing, and saves.
- [ ] Define visibility and draw ordering before adding glass, leaves, water, or other non-opaque materials.
- [ ] Define recipes, stacking, durability, tool suitability, and drops before exposing usable items.
- [ ] Verify texture repetition, chunk seams, placement orientation, inventory behavior, and save/restart round trips.

Start with a small wood and masonry set, then tool gathering and timed breaking.
Keep liquids, complex mechanisms, advanced combat, and magic as later increments
with their own behavior and tests rather than placeholder items in the hotbar.
