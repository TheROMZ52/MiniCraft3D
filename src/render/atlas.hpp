#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mc::atlas {

constexpr int TileSize = 16;
constexpr int Columns = 32;
constexpr int Width = TileSize * Columns;
constexpr int Height = TileSize;

enum Tile : int {
    GrassTop, Dirt, Stone, LogSide, Leaves, Sand, Snow, Ore, GrassSide, Planks,
    LogTop, TableTop, TableSide, Stick,
    WoodPickaxe, StonePickaxe, WoodAxe, StoneAxe, WoodShovel, StoneShovel, WoodSword, StoneSword,
    Crack0, Crack9 = Crack0 + 9
};

struct Rgba {
    std::uint8_t r = 0, g = 0, b = 0, a = 0;
};

using Pixels = std::vector<Rgba>;

inline std::uint32_t hashCell(int x, int y, int seed) {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u +
                      static_cast<std::uint32_t>(seed) * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return n ^ (n >> 16);
}

inline float rnd(int x, int y, int seed) {
    return static_cast<float>(hashCell(x, y, seed) & 0xFFFFu) / 65535.0f;
}

inline float tileNoise(float x, float y, int seed, int period) {
    float cell = static_cast<float>(TileSize) / static_cast<float>(period);
    float fx = x / cell;
    float fy = y / cell;
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float tx = fx - static_cast<float>(ix);
    float ty = fy - static_cast<float>(iy);
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    auto lattice = [&](int cx, int cy) {
        return rnd(((cx % period) + period) % period, ((cy % period) + period) % period, seed);
    };
    float a = lattice(ix, iy);
    float b = lattice(ix + 1, iy);
    float c = lattice(ix, iy + 1);
    float d = lattice(ix + 1, iy + 1);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

struct Rgb {
    int r, g, b;
};

template <std::size_t N>
inline Rgba pick(const std::array<Rgb, N>& palette, float t) {
    int index = static_cast<int>(std::clamp(t, 0.0f, 0.9999f) * static_cast<float>(N));
    const Rgb& c = palette[static_cast<std::size_t>(index)];
    return {static_cast<std::uint8_t>(c.r), static_cast<std::uint8_t>(c.g),
            static_cast<std::uint8_t>(c.b), 255};
}

inline Rgba solid(int r, int g, int b) {
    return {static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
            static_cast<std::uint8_t>(b), 255};
}

inline Rgba scaled(Rgba c, float k) {
    auto s = [&](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(v) * k, 0.0f, 255.0f));
    };
    return {s(c.r), s(c.g), s(c.b), c.a};
}

class Canvas {
public:
    explicit Canvas(Pixels& target, int tile) : pixels_(target), tile_(tile) {}

    void set(int x, int y, Rgba c) {
        if (x < 0 || y < 0 || x >= TileSize || y >= TileSize) return;
        pixels_[static_cast<std::size_t>(y * Width + tile_ * TileSize + x)] = c;
    }

    Rgba get(int x, int y) const {
        if (x < 0 || y < 0 || x >= TileSize || y >= TileSize) return {};
        return pixels_[static_cast<std::size_t>(y * Width + tile_ * TileSize + x)];
    }

    int tile() const { return tile_; }

private:
    Pixels& pixels_;
    int tile_;
};

inline void drawGrassTop(Canvas& cv) {
    static const std::array<Rgb, 6> palette = {{
        {84, 140, 46}, {94, 154, 52}, {104, 166, 60}, {114, 178, 68}, {124, 190, 78}, {136, 202, 90}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float t = 0.55f * rnd(x, y, 1) + 0.45f * tileNoise(static_cast<float>(x), static_cast<float>(y), 11, 8);
            cv.set(x, y, pick(palette, t));
        }
}

inline void drawDirtBase(Canvas& cv, int seed) {
    static const std::array<Rgb, 6> palette = {{
        {88, 62, 40}, {102, 73, 48}, {116, 83, 56}, {128, 93, 64}, {140, 102, 72}, {152, 112, 80}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float t = 0.55f * rnd(x, y, seed) + 0.45f * tileNoise(static_cast<float>(x), static_cast<float>(y), seed + 5, 8);
            Rgba c = pick(palette, t);
            float stone = rnd(x, y, seed + 9);
            if (stone > 0.955f) c = solid(150, 138, 126);
            else if (stone < 0.035f) c = solid(70, 48, 30);
            cv.set(x, y, c);
        }
}

inline void drawStoneBase(Canvas& cv, int seed) {
    static const std::array<Rgb, 6> palette = {{
        {98, 98, 98}, {110, 110, 110}, {122, 122, 122}, {132, 132, 132}, {142, 142, 142}, {152, 152, 152}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float t = 0.5f * rnd(x, y, seed) + 0.5f * tileNoise(static_cast<float>(x), static_cast<float>(y), seed + 3, 4);
            Rgba c = pick(palette, t);
            if (rnd(x, y, seed + 17) > 0.97f) c = scaled(c, 0.82f);
            cv.set(x, y, c);
        }
}

inline void drawGrassSide(Canvas& cv) {
    drawDirtBase(cv, 2);
    static const std::array<Rgb, 5> palette = {{
        {84, 140, 46}, {96, 156, 54}, {108, 170, 62}, {120, 184, 72}, {132, 198, 84}}};
    for (int x = 0; x < TileSize; ++x) {
        int depth = 3 + static_cast<int>(hashCell(x, 0, 41) % 3u);
        for (int y = 0; y < depth; ++y) {
            float t = 0.6f * rnd(x, y, 1) + 0.4f * tileNoise(static_cast<float>(x), static_cast<float>(y), 11, 8);
            Rgba c = pick(palette, t);
            if (y == depth - 1) c = scaled(c, 0.86f);
            cv.set(x, y, c);
        }
    }
}

inline void drawLogSide(Canvas& cv) {
    static const std::array<Rgb, 5> palette = {{
        {56, 42, 24}, {70, 53, 30}, {86, 66, 38}, {100, 78, 46}, {112, 88, 52}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float streak = tileNoise(static_cast<float>(x) * 1.0f, static_cast<float>(y) * 0.2f, 31, 16);
            float t = 0.55f * streak + 0.45f * rnd(x, y / 3, 32);
            Rgba c = pick(palette, t);
            if (x % 4 == 0) c = scaled(c, 0.82f);
            if (rnd(x, y, 33) > 0.94f) c = scaled(c, 0.7f);
            cv.set(x, y, c);
        }
}

inline void drawLogTop(Canvas& cv) {
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float d = std::max(std::fabs(static_cast<float>(x) - 7.5f), std::fabs(static_cast<float>(y) - 7.5f));
            float jitter = (rnd(x, y, 51) - 0.5f) * 0.18f;
            Rgba c;
            if (d > 6.4f) {
                c = scaled(solid(88, 66, 38), 0.9f + 0.2f * rnd(x, y, 52));
            } else {
                int ring = static_cast<int>(d);
                bool dark = (ring % 2) == 1;
                c = dark ? solid(150, 118, 70) : solid(178, 146, 90);
                if (d < 1.6f) c = solid(132, 100, 58);
                c = scaled(c, 1.0f + jitter);
            }
            cv.set(x, y, c);
        }
}

inline void drawLeaves(Canvas& cv) {
    static const std::array<Rgb, 5> palette = {{
        {32, 82, 20}, {42, 100, 26}, {54, 118, 32}, {66, 136, 40}, {80, 154, 50}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float t = 0.5f * rnd(x, y, 61) + 0.5f * tileNoise(static_cast<float>(x), static_cast<float>(y), 62, 8);
            Rgba c = pick(palette, t);
            float hole = rnd(x, y, 63);
            if (hole < 0.17f) c = {0, 0, 0, 0};
            else if (hole > 0.9f) c = scaled(c, 0.7f);
            cv.set(x, y, c);
        }
}

inline void drawSand(Canvas& cv) {
    static const std::array<Rgb, 5> palette = {{
        {196, 182, 138}, {208, 195, 150}, {219, 207, 162}, {228, 217, 172}, {236, 226, 184}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float t = 0.6f * rnd(x, y, 71) + 0.4f * tileNoise(static_cast<float>(x), static_cast<float>(y), 72, 8);
            Rgba c = pick(palette, t);
            if (rnd(x, y, 73) > 0.96f) c = scaled(c, 0.88f);
            cv.set(x, y, c);
        }
}

inline void drawSnow(Canvas& cv) {
    static const std::array<Rgb, 5> palette = {{
        {226, 233, 244}, {236, 241, 249}, {244, 247, 252}, {250, 252, 255}, {255, 255, 255}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            float t = 0.5f * rnd(x, y, 81) + 0.5f * tileNoise(static_cast<float>(x), static_cast<float>(y), 82, 8);
            cv.set(x, y, pick(palette, t));
        }
}

inline void drawOre(Canvas& cv) {
    drawStoneBase(cv, 91);
    static const std::array<Rgba, 3> dots = {{solid(222, 182, 150), solid(205, 160, 122), solid(176, 132, 98)}};
    for (int k = 0; k < 8; ++k) {
        int cx = 1 + static_cast<int>(hashCell(k, 1, 92) % 13u);
        int cy = 1 + static_cast<int>(hashCell(k, 2, 92) % 13u);
        cv.set(cx, cy, dots[0]);
        static const int offsets[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {1, 1}};
        for (int i = 0; i < 4; ++i) {
            if (hashCell(k, i + 3, 93) % 100u < 55u)
                cv.set(cx + offsets[i][0], cy + offsets[i][1], dots[1 + static_cast<int>(hashCell(k, i, 94) % 2u)]);
        }
    }
}

inline void drawPlanks(Canvas& cv) {
    static const std::array<Rgb, 5> palette = {{
        {140, 110, 66}, {152, 121, 74}, {163, 131, 82}, {172, 140, 90}, {181, 149, 98}}};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            int board = y / 4;
            float grain = tileNoise(static_cast<float>(x) * 0.35f, static_cast<float>(y), 101 + board, 16);
            float t = 0.5f * grain + 0.5f * rnd(x / 2, y, 102);
            Rgba c = pick(palette, t);
            if (y % 4 == 3) c = solid(104, 80, 46);
            else if (y % 4 == 0) c = scaled(c, 1.06f);
            int joint = (board % 2 == 0) ? 6 : 12;
            if (x == joint && y % 4 != 3) c = solid(112, 86, 50);
            cv.set(x, y, c);
        }
}

inline void drawTableTop(Canvas& cv) {
    drawPlanks(cv);
    for (int i = 0; i < TileSize; ++i) {
        cv.set(i, 0, solid(84, 58, 32));
        cv.set(i, 15, solid(84, 58, 32));
        cv.set(0, i, solid(84, 58, 32));
        cv.set(15, i, solid(84, 58, 32));
        cv.set(i, 1, solid(128, 96, 56));
        cv.set(i, 14, solid(128, 96, 56));
        cv.set(1, i, solid(128, 96, 56));
        cv.set(14, i, solid(128, 96, 56));
    }
    for (int i = 2; i < 14; ++i) {
        cv.set(5, i, solid(104, 76, 42));
        cv.set(10, i, solid(104, 76, 42));
        cv.set(i, 5, solid(104, 76, 42));
        cv.set(i, 10, solid(104, 76, 42));
    }
}

inline void drawTableSide(Canvas& cv) {
    drawPlanks(cv);
    for (int x = 0; x < TileSize; ++x) {
        cv.set(x, 0, solid(84, 58, 32));
        cv.set(x, 1, solid(122, 92, 54));
        cv.set(x, 2, solid(122, 92, 54));
        cv.set(x, 3, solid(96, 68, 38));
        cv.set(x, 15, solid(84, 58, 32));
    }
    for (int y = 0; y < TileSize; ++y) {
        cv.set(0, y, solid(84, 58, 32));
        cv.set(15, y, solid(84, 58, 32));
    }
    for (int y = 6; y <= 12; ++y) {
        cv.set(3, y, solid(176, 176, 180));
        cv.set(4, y, solid(138, 138, 144));
        if (y % 2 == 0) cv.set(5, y, solid(110, 110, 116));
    }
    for (int x = 8; x <= 12; ++x) cv.set(x, 6, solid(160, 160, 166));
    for (int x = 9; x <= 12; ++x) cv.set(x, 7, solid(116, 116, 122));
    for (int y = 8; y <= 12; ++y) cv.set(11, y, solid(86, 60, 34));
}

struct SpritePalette {
    Rgba outline, dark, mid, light;
};

inline void spritePixel(Canvas& cv, int x, int y, Rgba c) { cv.set(x, y, c); }

inline void spriteOutline(Canvas& cv, Rgba outline) {
    std::array<Rgba, TileSize * TileSize> snapshot{};
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) snapshot[static_cast<std::size_t>(y * TileSize + x)] = cv.get(x, y);
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            if (snapshot[static_cast<std::size_t>(y * TileSize + x)].a) continue;
            bool near = false;
            for (int dy = -1; dy <= 1 && !near; ++dy)
                for (int dx = -1; dx <= 1 && !near; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= TileSize || ny >= TileSize) continue;
                    if (snapshot[static_cast<std::size_t>(ny * TileSize + nx)].a) near = true;
                }
            if (near) cv.set(x, y, outline);
        }
}

inline void drawHandle(Canvas& cv, int x0, int y0, int x1, int y1) {
    const Rgba light = solid(150, 108, 58);
    const Rgba dark = solid(104, 72, 38);
    int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i <= steps; ++i) {
        float t = steps ? static_cast<float>(i) / static_cast<float>(steps) : 0.0f;
        int x = static_cast<int>(std::lround(static_cast<float>(x0) + static_cast<float>(x1 - x0) * t));
        int y = static_cast<int>(std::lround(static_cast<float>(y0) + static_cast<float>(y1 - y0) * t));
        cv.set(x, y, light);
        cv.set(x + 1, y, dark);
    }
}

inline void paintPoints(Canvas& cv, const int (*pts)[2], int count, Rgba c) {
    for (int i = 0; i < count; ++i) cv.set(pts[i][0], pts[i][1], c);
}

inline SpritePalette materialPalette(bool stone) {
    if (stone)
        return {solid(48, 48, 52), solid(104, 104, 108), solid(136, 136, 140), solid(172, 172, 176)};
    return {solid(70, 48, 24), solid(138, 106, 60), solid(168, 134, 82), solid(196, 162, 104)};
}

inline void drawStick(Canvas& cv) {
    drawHandle(cv, 3, 13, 12, 4);
    spriteOutline(cv, solid(40, 28, 16));
}

inline void drawPickaxe(Canvas& cv, bool stone) {
    SpritePalette p = materialPalette(stone);
    drawHandle(cv, 2, 14, 11, 5);
    static const int outer[][2] = {{4, 4}, {5, 3}, {6, 2}, {7, 2}, {8, 2}, {9, 2}, {10, 3}, {11, 3}, {12, 4}, {13, 5}, {13, 6}, {14, 7}};
    static const int inner[][2] = {{5, 4}, {6, 3}, {7, 3}, {8, 3}, {9, 3}, {10, 4}, {11, 4}, {12, 5}, {12, 6}, {13, 7}};
    paintPoints(cv, outer, 12, p.light);
    paintPoints(cv, inner, 10, p.mid);
    cv.set(9, 3, p.dark);
    cv.set(12, 6, p.dark);
    spriteOutline(cv, p.outline);
}

inline void drawAxe(Canvas& cv, bool stone) {
    SpritePalette p = materialPalette(stone);
    drawHandle(cv, 2, 14, 11, 5);
    static const int blade[][2] = {{8, 2}, {9, 2}, {10, 2}, {11, 2}, {12, 2}, {7, 3}, {8, 3}, {9, 3}, {10, 3}, {11, 3}, {12, 3},
                                   {6, 4}, {7, 4}, {8, 4}, {9, 4}, {10, 4}, {11, 4}, {6, 5}, {7, 5}, {8, 5}, {9, 5}, {7, 6}, {8, 6}};
    paintPoints(cv, blade, 23, p.mid);
    static const int edge[][2] = {{8, 2}, {9, 2}, {7, 3}, {6, 4}, {6, 5}, {7, 6}};
    paintPoints(cv, edge, 6, p.light);
    static const int shade[][2] = {{12, 2}, {12, 3}, {11, 4}, {9, 5}, {8, 6}};
    paintPoints(cv, shade, 5, p.dark);
    spriteOutline(cv, p.outline);
}

inline void drawShovel(Canvas& cv, bool stone) {
    SpritePalette p = materialPalette(stone);
    drawHandle(cv, 2, 14, 10, 6);
    for (int i = 0; i < 4; ++i) {
        cv.set(10 + i, 5 - i, p.mid);
        cv.set(11 + i, 5 - i, p.mid);
        cv.set(10 + i, 4 - i, p.light);
        cv.set(11 + i, 4 - i, p.mid);
        cv.set(11 + i, 6 - i, p.dark);
    }
    cv.set(14, 1, p.light);
    cv.set(14, 2, p.mid);
    cv.set(13, 1, p.light);
    spriteOutline(cv, p.outline);
}

inline void drawSword(Canvas& cv, bool stone) {
    SpritePalette p = materialPalette(stone);
    drawHandle(cv, 2, 14, 5, 11);
    for (int i = 0; i < 9; ++i) {
        cv.set(6 + i, 10 - i, p.light);
        cv.set(7 + i, 10 - i, p.mid);
        if (i > 0) cv.set(5 + i, 10 - i, p.light);
    }
    cv.set(14, 2, p.light);
    cv.set(15, 1, p.mid);
    static const int guard[][2] = {{3, 9}, {4, 8}, {5, 12}, {6, 13}, {4, 10}, {5, 11}, {6, 11}, {7, 12}, {5, 9}};
    paintPoints(cv, guard, 9, solid(120, 88, 48));
    spriteOutline(cv, p.outline);
}

inline void crackLine(Canvas& cv, float x, float y, float angle, int length, int seed, int stage) {
    for (int i = 0; i < length; ++i) {
        cv.set(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), solid(0, 0, 0));
        angle += (rnd(seed, i, 201) - 0.5f) * 0.9f;
        x += std::cos(angle);
        y += std::sin(angle);
        if (i == length / 2 && stage >= 4 && hashCell(seed, i, 203) % 2u == 0u)
            crackLine(cv, x, y, angle + (rnd(seed, 9, 204) > 0.5f ? 1.1f : -1.1f), length / 2, seed + 31, 0);
    }
}

inline void drawCrack(Canvas& cv, int stage) {
    constexpr int rays = 7;
    for (int k = 0; k < rays; ++k) {
        float angle = (static_cast<float>(k) / rays) * 6.2831853f + rnd(k, 1, 200) * 0.5f;
        int length = std::min(2 + stage * 7 / 9 + static_cast<int>(hashCell(k, 4, 200) % 3u), 8);
        if (stage == 0 && k > 2) continue;
        if (stage < 3 && k > 4) continue;
        crackLine(cv, 7.5f, 7.5f, angle, length, k, stage);
    }
}

inline Pixels generate() {
    Pixels pixels(static_cast<std::size_t>(Width * Height));
    auto make = [&](int tile) { return Canvas(pixels, tile); };
    { Canvas c = make(GrassTop); drawGrassTop(c); }
    { Canvas c = make(Dirt); drawDirtBase(c, 2); }
    { Canvas c = make(Stone); drawStoneBase(c, 21); }
    { Canvas c = make(LogSide); drawLogSide(c); }
    { Canvas c = make(Leaves); drawLeaves(c); }
    { Canvas c = make(Sand); drawSand(c); }
    { Canvas c = make(Snow); drawSnow(c); }
    { Canvas c = make(Ore); drawOre(c); }
    { Canvas c = make(GrassSide); drawGrassSide(c); }
    { Canvas c = make(Planks); drawPlanks(c); }
    { Canvas c = make(LogTop); drawLogTop(c); }
    { Canvas c = make(TableTop); drawTableTop(c); }
    { Canvas c = make(TableSide); drawTableSide(c); }
    { Canvas c = make(Stick); drawStick(c); }
    { Canvas c = make(WoodPickaxe); drawPickaxe(c, false); }
    { Canvas c = make(StonePickaxe); drawPickaxe(c, true); }
    { Canvas c = make(WoodAxe); drawAxe(c, false); }
    { Canvas c = make(StoneAxe); drawAxe(c, true); }
    { Canvas c = make(WoodShovel); drawShovel(c, false); }
    { Canvas c = make(StoneShovel); drawShovel(c, true); }
    { Canvas c = make(WoodSword); drawSword(c, false); }
    { Canvas c = make(StoneSword); drawSword(c, true); }
    for (int stage = 0; stage < 10; ++stage) { Canvas c = make(Crack0 + stage); drawCrack(c, stage); }
    return pixels;
}

inline Rgba averageColor(const Pixels& pixels, int tile) {
    long r = 0, g = 0, b = 0, n = 0;
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            const Rgba& p = pixels[static_cast<std::size_t>(y * Width + tile * TileSize + x)];
            if (!p.a) continue;
            r += p.r; g += p.g; b += p.b; ++n;
        }
    if (!n) return {128, 128, 128, 255};
    return {static_cast<std::uint8_t>(r / n), static_cast<std::uint8_t>(g / n),
            static_cast<std::uint8_t>(b / n), 255};
}

}
