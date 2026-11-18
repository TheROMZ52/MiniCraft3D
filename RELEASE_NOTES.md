# MiniCraft3D 28.1 release notes

## Render distance
- Range 2-32 chunks, default 12; fog starts at 70% of the distance and fully hides the edge like Minecraft
- Chunks load nearest-first with a per-frame budget instead of all at once, so moving no longer stutters
- Mesh rebuilds are time-sliced (5 ms per frame), prioritised by distance and wait until all four neighbours exist
- New padded-array mesher: no hash lookups per block, skips air above the surface and fully buried blocks
- Rendering iterates loaded meshes directly (no per-frame sort); corners that are always hidden by fog are skipped
- Far plane follows the render distance

## World generation
- Trees are placed on a spaced grid (one candidate per 6x6 cell) instead of rolling every column
- Forest biome is rarer and less dense; plains and snow have occasional trees
- Fixes the world being covered by merged leaf canopies

## Inventory
- Exact Minecraft survival inventory layout: armor column, player preview, 2x2 crafting grid with result slot, 3x9 storage and 9-slot hotbar
- Cursor stack: left click picks up or places, right click splits or places one, shift-click moves between hotbar and storage or crafts the whole stack
- Slot hover highlight, item name tooltip, durability bars only on damaged tools, dimmed background
- Items on the cursor and in the crafting grid return to the inventory (or drop) when the screen closes
- Minecraft-style hotbar HUD with white selector, 9 slots and keys 1-9
- Old saves (32 slots) are migrated automatically; new saves use the 36-slot format

## Technical
- Inventory is now 36 slots; added `Inventory::setSlot`, `Crafting::matchGrid`
- Added world tests for budgeted streaming and tree density, gameplay tests for grid matching
