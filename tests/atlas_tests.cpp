#include "../src/render/atlas.hpp"
#include <cstdlib>
#include <iostream>

namespace atl = mc::atlas;

static int failures = 0;

static void check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

static int opaqueCount(const atl::Pixels& pixels, int tile) {
    int count = 0;
    for (int y = 0; y < atl::TileSize; ++y)
        for (int x = 0; x < atl::TileSize; ++x)
            if (pixels[static_cast<std::size_t>(y * atl::Width + tile * atl::TileSize + x)].a) ++count;
    return count;
}

int main() {
    const atl::Pixels pixels = atl::generate();
    check(pixels.size() == static_cast<std::size_t>(atl::Width * atl::Height), "atlas size");
    check(atl::Width == 512 && atl::Height == 16, "atlas dimensions");

    for (int tile : {atl::GrassTop, atl::Dirt, atl::Stone, atl::LogSide, atl::Sand, atl::Snow,
                     atl::Ore, atl::GrassSide, atl::Planks, atl::LogTop, atl::TableTop, atl::TableSide})
        check(opaqueCount(pixels, tile) == 256, "block tiles are fully opaque");

    const int leaves = opaqueCount(pixels, atl::Leaves);
    check(leaves > 150 && leaves < 256, "leaves have transparent holes");

    for (int tile = atl::Stick; tile <= atl::StoneSword; ++tile) {
        const int count = opaqueCount(pixels, tile);
        check(count > 15 && count < 200, "item sprites are partially transparent");
    }

    int previous = 0;
    for (int stage = 0; stage < 10; ++stage) {
        const int count = opaqueCount(pixels, atl::Crack0 + stage);
        check(count > 0, "crack stage has pixels");
        check(count >= previous, "crack stages grow");
        previous = count;
    }

    const atl::Rgba grass = atl::averageColor(pixels, atl::GrassTop);
    check(grass.g > grass.r && grass.g > grass.b, "grass top is green");
    const atl::Rgba sand = atl::averageColor(pixels, atl::Sand);
    check(sand.r > 180 && sand.b < sand.r, "sand is warm and bright");

    const atl::Pixels again = atl::generate();
    bool same = true;
    for (std::size_t i = 0; i < pixels.size(); ++i)
        if (pixels[i].r != again[i].r || pixels[i].g != again[i].g || pixels[i].b != again[i].b || pixels[i].a != again[i].a)
            same = false;
    check(same, "atlas generation is deterministic");

    if (failures == 0) std::cout << "Atlas tests passed\n";
    return failures == 0 ? 0 : 1;
}
