# MiniCraft3D 27.3

A Minecraft-inspired voxel survival sandbox written in C++17 and OpenGL 3.3 Core. No game engine.

## Gameplay

- 32-slot inventory with an 8-slot hotbar and 64-item stack limits
- Tool items: wooden pickaxe, stone pickaxe, wooden axe, wooden shovel and wooden sword
- Individual tool durability; broken tools are removed from the inventory
- Mining progress depends on block hardness and tool efficiency
- Stone and ore require a pickaxe to drop their block item
- Multi-ingredient recipe crafting for tools
- Health and hunger with sprint exhaustion, natural regeneration and starvation
- Fall damage and automatic respawn
- Block drops, proximity pickup and inventory-aware placement
- Inventory, tool durability, health, hunger and drops persist in world saves
- Backward-compatible loading of 27.1 and 27.2 world saves

## World and rendering

- 16 independent chunk meshes with separate GPU vertex buffers
- Frustum culling and hidden-face removal across chunk borders
- Vertex ambient occlusion
- Procedural 10-tile pixel-art texture atlas
- Procedural plains, forest, desert and snowy regions
- Underground caves and small ore deposits
- First-person movement, collision, jumping and sprinting
- Animated daylight, sky tint and distance fog
- Health and hunger bars; tool durability bars in inventory slots

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
| Hold left click | Mine a targeted block |
| Right click | Place the selected hotbar block |
| 1-8 | Select hotbar slot |
| Mouse wheel | Change selected hotbar slot |
| E | Open or close inventory |
| Click inventory slot | Swap with selected hotbar slot |
| C | Craft the first available recipe |
| F5 | Save world |
| F9 | Load world |
| Esc | Exit |

Mining takes time based on block hardness and the held tool. Pickaxes mine stone and ore efficiently and are required to collect them. Axes are effective on logs; shovels are effective on soil and sand. Every tool has its own durability and breaks when exhausted.

Crafting recipes include planks, sticks, a crafting table, wooden and stone pickaxes, a wooden axe, shovel and sword. Ingredients are consumed only when the output can fit in the inventory.

Health and hunger are shown at the top-left of the screen. Sprinting consumes hunger faster; high hunger enables natural regeneration, while starvation causes damage. Hard falls cause damage. At zero health the player respawns with full health and reduced hunger.

Worlds are saved to `MiniCraft3D.world` on exit. Existing 27.1 and 27.2 saves remain loadable and are upgraded to the 27.3 format on the next save.

## Tests

GitHub Actions runs standalone tests for inventory stacking, crafting, tool durability, harvest requirements and survival stats before compiling the Windows game.
