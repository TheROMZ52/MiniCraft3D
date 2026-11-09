# MiniCraft3D 27.1

A small Minecraft-inspired sandbox written in C++17 and OpenGL 3.3 Core. No game engine.

## Features

- First-person movement, mouse look, jumping and sprinting
- Procedural 64x64 terrain with hills, trees, sand patches and decorative blocks
- Eight block types and a generated pixel-art texture atlas
- Hidden-face culling and a single combined world mesh draw call
- Break/place blocks, hotbar selection and scroll-wheel switching
- Directional lighting, a slow day/night tint, distance fog and target outline
- Live FPS and player coordinates in the window title

## Requirements

- Windows 10/11 x64
- MSYS2 UCRT64
- GCC/G++
- GLFW, GLEW and GLM

Install libraries in the MSYS2 UCRT64 terminal:

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
| Esc | Exit |

Textures are generated procedurally at startup and packed into a nearest-filtered atlas; no external image loader is required.
