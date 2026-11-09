# MiniCraft3D 27.2

A Minecraft-inspired voxel sandbox written in C++17 and OpenGL 3.3 Core. No game engine.

## Gameplay

- 32-slot inventory with an 8-slot hotbar
- Stack merging with a 64-item stack limit
- Block drops with simple physics and proximity pickup
- Item placement consumes inventory stacks
- Recipe-based crafting: logs to planks, planks to sticks, and planks to a crafting table
- Inventory and dropped items are saved with the world
- Backward-compatible loading of 27.1 world saves

## World and rendering

- 16 independent chunk meshes with separate GPU vertex buffers
- Frustum culling and hidden-face removal across chunk borders
- Vertex ambient occlusion
- Procedural 10-tile pixel-art texture atlas
- Procedural plains, forest, desert and snowy regions
- Underground caves and small ore deposits
- First-person movement, collision, jumping and sprinting
- Block placement and breaking with a target outline
- Animated daylight, sky tint and distance fog

## Requirements

- Windows 10/11 x64
- MSYS2 UCRT64
- GCC/G++
- GLFW, GLEW and GLM

Install the required packages in the MSYS2 UCRT64 terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-glm
```

## Build

Run `build.bat` from the project directory. Then launch `MiniCraft3D.exe`.

## Controls

| Key | Action |
|---|---|
| W A S D | Move |
| Mouse | Look |
| Space | Jump |
| Left Shift | Sprint |
| Left click | Break block and drop its item |
| Right click | Place the selected hotbar block |
| 1-8 | Select hotbar slot |
| Mouse wheel | Change selected hotbar slot |
| E | Open or close the inventory panel |
| C | Craft the first available recipe |
| F5 | Save world |
| F9 | Load world |
| Esc | Exit |

The inventory panel displays the 24 backpack slots above the hotbar. Click a backpack slot to swap it with the selected hotbar slot; click a hotbar slot to select it. Items are collected automatically when the player gets close to a drop. Crafting consumes ingredients from any inventory slot.

Worlds are saved to `MiniCraft3D.world` on exit. Existing 27.1 saves remain loadable; the next save upgrades them to the 27.2 format.

Textures are generated procedurally. No external image assets or image loader are required.

## Tests

The gameplay module has standalone tests for stack limits, item removal and crafting recipes. GitHub Actions runs these tests before compiling the Windows game.
