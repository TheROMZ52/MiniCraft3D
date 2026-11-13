# MiniCraft3D 28.0

A Minecraft-inspired voxel survival sandbox written in C++17 and OpenGL 3.3 Core. Built from scratch without a game engine.

## Gameplay

- Main menu, world selection, multiple save slots and Settings
- 32-slot inventory, 8-slot hotbar and interactive recipe book
- Wooden and stone tools, durability, mining progression and harvest requirements
- Health, hunger, sprint exhaustion, regeneration and starvation
- Block drops, pickup, placement and persistent chunk saves
- First-person hand swing and walk bobbing animations

## Infinite world and rendering

- Procedurally generated world with no horizontal world boundary
- 16 x 16 chunks with a 64-block vertical range
- Dynamic chunk streaming around the player, including negative world coordinates
- Automatic chunk unloading outside the active streaming radius
- Render distance setting from 4 to 16 chunks, radial visibility filtering and frustum culling
- OpenGL mesh generation per loaded chunk with hidden-face removal
- Neighbor mesh invalidation when blocks change or adjacent chunks stream in
- Plains, forest, desert and snowy regions, caves, ores and trees
- Chunk data saved separately from world/player metadata
- Legacy 27.1, 27.2 and 27.3 saves are imported into the chunk-based world format

## Minecraft-style visuals (28.0)

- Procedurally generated 16 x 16 pixel-art textures: grass, dirt, stone, logs with bark and rings, leaves with cut-out holes, sand, snow, ore, planks, crafting table, sticks and all wooden and stone tools
- Classic per-face shading, vertex ambient occlusion and distance fog that matches the sky
- Sky with a square sun and moon, stars, drifting blocky clouds, sunset colors and a full day and night cycle
- Block-break crack animation, black selection outline and breaking particles
- First-person held blocks and extruded 3D tools, spinning item drops with textured models
- Isometric block icons and sprite icons in the hotbar and inventory, bevelled slots, grey inventory panel
- Dirt-tile menu background and bevelled buttons with text shadows

## Texture atlas

The game builds its atlas at startup. If `textures/atlas.ppm` exists and is valid it replaces the built-in atlas. See `textures/README.md` for the format and how to regenerate it.

The atlas is a PPM image of exactly **512 x 16 pixels** with thirty-two horizontal 16 x 16 tiles. Magenta (255, 0, 255) marks transparent pixels.

| Tile | Texture |
|---:|---|
| 0 | Grass top |
| 1 | Dirt |
| 2 | Stone |
| 3 | Log side |
| 4 | Leaves |
| 5 | Sand |
| 6 | Snow |
| 7 | Ore |
| 8 | Grass side |
| 9 | Planks |
| 10 | Log top |
| 11 | Crafting table top |
| 12 | Crafting table side |
| 13 | Stick |
| 14-15 | Wooden and stone pickaxe |
| 16-17 | Wooden and stone axe |
| 18-19 | Wooden and stone shovel |
| 20-21 | Wooden and stone sword |
| 22-31 | Block break stages 0-9 |

## Settings

Settings are saved to `MiniCraft3D.cfg` and include FOV, mouse sensitivity, render distance, debug information and view bobbing.

## Requirements

- Windows 10/11 x64
- MSYS2 UCRT64
- GCC/G++
- GLFW, GLEW and GLM

Install packages in the MSYS2 UCRT64 terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-glm
```

## Build and update

Run `build.bat` from the project directory, then launch `MiniCraft3D.exe`.

Run `update.bat` to download the executable from the latest published GitHub release. Releases are published when a `v*` tag is pushed.

## Controls

| Key | Action |
|---|---|
| W A S D | Move |
| Mouse | Look |
| Space | Jump |
| Left Shift | Sprint |
| Left click (hold) | Mine |
| Right click | Place block |
| 1-8 | Select hotbar slot |
| Mouse wheel | Change selected slot |
| E | Inventory |
| R | Recipe book |
| C | Quick-craft first available recipe |
| F5 | Save world |
| F9 | Load world |
| Esc | Pause / resume / back |

## Tests

GitHub Actions runs gameplay-system tests, infinite-world coordinate and terrain tests, cross-chunk mesh invalidation tests, streaming/unloading tests, chunk persistence tests and texture atlas tests before compiling the Windows executable.
