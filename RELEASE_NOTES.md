# MiniCraft3D 28.0 release notes

## Visuals
- New Minecraft-style 16 x 16 pixel-art texture atlas (32 tiles), generated in code and exported to `textures/atlas.ppm`
- New tiles: log top rings, crafting table, sticks, six tools, ten block-break stages
- Leaves are cut-out textures with holes; grass sides have an irregular green fringe
- Fixed texture orientation on block sides so grass fringes sit at the top
- Classic Minecraft face shading, softer ambient occlusion, fog that follows the sky colour
- New sky: square sun and moon, stars, drifting blocky clouds, sunset gradient, day and night cycle (8 minutes)
- Block-break cracks, black selection outline, break particles
- Held blocks and extruded 3D tools in first person; hand no longer clips into walls
- Item drops render as spinning textured blocks and tools

## Interface
- Dirt-tile menu background, bevelled grey buttons, text shadows
- Isometric block icons and sprite icons in hotbar and inventory, bevelled slots, white selection frame
- Grey inventory panel; removed the mining progress bar in favour of break cracks

## Technical
- Atlas moved to `src/render/atlas.hpp`; `tools/atlas_dump.cpp` regenerates `textures/atlas.ppm`
- External atlas must now be 512 x 16 and uses magenta as the transparency key
- Added `tests/atlas_tests.cpp` and a CI step for it
- Version bumped to 28.0
