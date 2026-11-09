# MiniCraft3D 27.1

A Minecraft-inspired voxel sandbox written from scratch in C++17 and OpenGL 3.3 Core. No game engine.

## Current features

- Chunk-based mesh storage: 16 independent chunks, each with its own GPU vertex buffer
- Frustum culling: chunks outside the camera view are skipped
- Hidden-face removal across chunk borders
- Vertex ambient occlusion for softer voxel corners
- Procedural 9-tile pixel-art texture atlas generated at startup
- Procedural plains, forest, desert and snowy regions
- Underground caves and small ore deposits
- First-person controls, collision, jumping and sprinting
- Block placement and breaking with a target outline
- Eight hotbar slots and mouse-wheel selection
- Animated daylight, sky tint and distance fog
- World persistence in `MiniCraft3D.world`
- FPS, visible-chunk count and player coordinates in the window title

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
| Left click | Break block |
| Right click | Place block |
| 1-8 | Select block |
| Mouse wheel | Change selected block |
| F5 | Save world |
| F9 | Load world |
| Esc | Exit |

The world is saved automatically when the game exits. Keep `MiniCraft3D.world` beside the executable to load the same world next time.

Textures are generated procedurally and packed into a nearest-filtered atlas. No external image loader or texture files are required.
