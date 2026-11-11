# MiniCraft3D 27.5

A Minecraft-inspired voxel survival sandbox written in C++17 and OpenGL 3.3 Core. Built from scratch without a game engine.

## Gameplay

- Main menu with Singleplayer, world selection, multiple save slots, Settings and Quit
- 32-slot inventory and 8-slot hotbar
- Interactive recipe book (R) and quick crafting (C)
- Wooden and stone pickaxes, axes, shovels and swords
- Tool durability, mining progression and harvest requirements
- Health and hunger, sprint exhaustion, regeneration and starvation
- Block drops, pickup, placement and persistent world saves
- First-person hand swing and walk bobbing animations

## World and rendering

- 128 x 32 x 128 procedural voxel world (four times the previous horizontal area)
- Up to five saved worlds are shown in the world selector; new worlds receive unique save files
- 64 independently meshed chunks with frustum culling and hidden-face removal
- Vertex ambient occlusion, animated daylight and distance fog
- Built-in pixel-art texture atlas with optional external PPM atlas
- Plains, forest, desert and snowy regions, caves, ores and trees
- Legacy 27.1, 27.2 and 27.3 saves are migrated into the expanded world

## Texture atlas

The game looks for `textures/atlas.ppm` at startup. If it is missing or invalid, the built-in atlas is used.

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

Keep each tile 16 x 16. Use nearest-neighbor filtering and avoid adding gaps between tiles.

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

## Build

Run `build.bat` from the project directory, then launch `MiniCraft3D.exe`.

## Update

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

GitHub Actions runs standalone inventory, crafting, tool durability, harvest requirement and survival-stat tests before compiling the Windows executable.
