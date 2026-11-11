#include "../src/world/infinite_world.hpp"

#include <cassert>
#include <filesystem>

int main() {
    using namespace mc::world;

    assert(floorDiv(0) == 0);
    assert(floorDiv(15) == 0);
    assert(floorDiv(16) == 1);
    assert(floorDiv(-1) == -1);
    assert(floorDiv(-16) == -1);
    assert(floorDiv(-17) == -2);
    assert(positiveMod(-1) == 15);
    assert(positiveMod(-16) == 0);

    const auto origin = InfiniteWorld::chunkOf(-1, -17);
    assert(origin.x == -1 && origin.z == -2);

    const auto directory = std::filesystem::temp_directory_path() / "minicraft3d-infinite-world-tests";
    std::error_code error;
    std::filesystem::remove_all(directory, error);

    InfiniteWorld world(directory);
    const auto surface = InfiniteWorld::heightAt(-1, -1);
    assert(surface > 0 && surface < WorldHeight);
    assert(world.getBlock(-1, surface, -1) != 0);
    assert(world.getBlock(-1, WorldHeight, -1) == 0);
    assert(world.getBlock(-1, -1, -1) == 3);

    assert(world.setBlock(-1, surface + 1, -1, 9));
    assert(world.getBlock(-1, surface + 1, -1) == 9);
    assert(!world.setBlock(-1, surface + 1, -1, 9));

    world.updateStreaming(0, 0, 3);
    assert(world.isLoaded({0, 0}));
    assert(world.isLoaded({-1, -1}));
    assert(world.loadedCount() <= 81);
    const auto visible = world.visibleChunks(0, 0, 2);
    assert(!visible.empty());
    for (const auto& pos : visible)
        assert(pos.x * pos.x + pos.z * pos.z <= 4);

    world.updateStreaming(1024, -1024, 2);
    assert(world.loadedCount() <= 41);
    assert(world.saveAll());

    InfiniteWorld reloaded(directory);
    assert(reloaded.getBlock(-1, surface + 1, -1) == 9);

    std::filesystem::remove_all(directory, error);
    return 0;
}
