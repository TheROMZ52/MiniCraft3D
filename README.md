# MiniCraft3D 27.6

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
- Vertex ambient occlusion, animated daylight and distance fog
- Plains, forest, desert and snowy regions, caves, ores and trees
- Chunk data saved separately from world/player metadata
- Legacy 27.1, 27.2 and 27.3 saves are imported into the chunk-based world format

## Texture atlas

The game loads `textures/atlas.ppm` at startup. If it is missing or invalid, the built-in atlas is used.

The external atlas must be a PPM P3 or P6 image exactly **160 x 16 pixels**, containing ten horizontal 16 x 16 tiles in this order:

| Tile | Texture |
|---:|---|
| 0 | Grass top |
| 1 | Dirt |
| 2 | Stone |
| 3 | Log |
| 4 | Leaves |
| 5 | Sand |
| 6 | Snow |
| 7 | Ore |
| 8 | Grass side |
| 9 | Planks |

Keep each tile 16 x 16. The supplied atlas uses pixel-art shading and nearest-neighbor filtering; avoid gaps between tiles.

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

GitHub Actions runs gameplay-system tests, infinite-world coordinate and terrain tests, cross-chunk mesh invalidation tests, streaming/unloading tests and chunk persistence tests before compiling the Windows executable.
