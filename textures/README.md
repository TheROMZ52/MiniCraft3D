# Texture atlas

The game generates a Minecraft-style 16 x 16 pixel-art atlas at startup. `atlas.ppm` is the same atlas exported to disk and, when present, replaces the built-in one.

- Canvas: 512 x 16 pixels (PPM P6 or P3, maximum value 255)
- Layout: thirty-two 16 x 16 tiles in one horizontal row
- Transparency: pure magenta (255, 0, 255) pixels are transparent
- Keep the canvas dimensions and tile boundaries unchanged

Regenerate the file from the built-in generator:

```bash
g++ tools/atlas_dump.cpp -o atlas_dump -std=c++17
./atlas_dump textures/atlas.ppm
```

If the file is missing or has an unsupported format, the built-in atlas is used.
