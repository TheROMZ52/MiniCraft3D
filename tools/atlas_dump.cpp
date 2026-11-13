#include "../src/render/atlas.hpp"
#include <cstdio>
#include <fstream>

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "textures/atlas.ppm";
    auto pixels = mc::atlas::generate();
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << mc::atlas::Width << " " << mc::atlas::Height << "\n255\n";
    for (const auto& p : pixels) {
        unsigned char rgb[3] = {p.r, p.g, p.b};
        if (!p.a) { rgb[0] = 255; rgb[1] = 0; rgb[2] = 255; }
        out.write(reinterpret_cast<const char*>(rgb), 3);
    }
    return out ? 0 : 1;
}
