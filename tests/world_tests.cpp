#include "../src/world/infinite_world.hpp"

#include <cassert>
#include <filesystem>
#include <algorithm>

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

    InfiniteWorld streaming(directory / "streaming");
    streaming.updateStreaming(0, 0, 2);
    assert(streaming.isLoaded({2, 0}));
    assert(!streaming.isLoaded({3, 0}));
    streaming.markMeshClean({2, 0});
    streaming.getBlock(48, surface, 0);
    assert(streaming.meshDirty({2, 0}));

    world.updateStreaming(0, 0, 3);
    assert(world.isLoaded({0, 0}));
    assert(world.isLoaded({-1, -1}));
    assert(world.loadedCount() <= 81);
    const auto visible = world.visibleChunks(0, 0, 2);
    assert(!visible.empty());
    for (const auto& pos : visible)
        assert(pos.x * pos.x + pos.z * pos.z <= 4);

    world.updateStreaming(0, 0, 3);
    assert(world.meshDirty({0, 0}));
    world.markMeshClean({0, 0});
    assert(!world.meshDirty({0, 0}));
    world.markMeshClean({1, 0});
    world.markMeshClean({0, 0});
    assert(world.setBlock(15, surface, 0, 9));
    assert(world.meshDirty({0, 0}));
    assert(world.meshDirty({1, 0}));
    world.markMeshClean({0, 0});
    world.markMeshClean({-1, 0});
    assert(world.setBlock(0, surface, 0, 9));
    assert(world.meshDirty({0, 0}));
    assert(world.meshDirty({-1, 0}));

    const auto loaded = world.loadedChunkPositions();
    assert(loaded.size() == world.loadedCount());
    assert(std::is_sorted(loaded.begin(), loaded.end(), [](const ChunkPos& a, const ChunkPos& b) {
        return a.z == b.z ? a.x < b.x : a.z < b.z;
    }));

    world.updateStreaming(1024, -1024, 2);
    assert(world.loadedCount() <= 41);
    assert(world.saveAll());

    InfiniteWorld reloaded(directory);
    assert(reloaded.getBlock(-1, surface + 1, -1) == 9);

    {
        const std::filesystem::path budgetDirectory = directory.string() + "_budget";
        std::filesystem::remove_all(budgetDirectory, error);
        InfiniteWorld budgeted(budgetDirectory);
        assert(budgeted.updateStreaming(0, 0, 8, 5) == 5);
        assert(budgeted.loadedCount() == 5);
        assert(budgeted.isLoaded({0, 0}));
        assert(!budgeted.isLoaded({8, 0}));
        assert(budgeted.updateStreaming(0, 0, 8, 5) == 5);
        assert(budgeted.loadedCount() == 10);
        assert(budgeted.peekChunk({0, 0}) != nullptr);
        assert(budgeted.peekChunk({50, 50}) == nullptr);
        assert(budgeted.hasLoadedNeighbors({0, 0}) == (budgeted.isLoaded({1, 0}) && budgeted.isLoaded({-1, 0}) &&
                                                       budgeted.isLoaded({0, 1}) && budgeted.isLoaded({0, -1})));
        budgeted.updateStreaming(0, 0, 4);
        assert(budgeted.hasLoadedNeighbors({0, 0}));
        std::filesystem::remove_all(budgetDirectory, error);
    }

    {
        const std::filesystem::path forestDirectory = directory.string() + "_trees";
        std::filesystem::remove_all(forestDirectory, error);
        InfiniteWorld forest(forestDirectory);
        long logBlocks = 0;
        int sampled = 0;
        for (int cx = -24; cx < 24; cx += 3) {
            for (int cz = -24; cz < 24; cz += 3) {
                forest.getBlock(cx * ChunkSize, 10, cz * ChunkSize);
                const Chunk* chunk = forest.peekChunk({cx, cz});
                assert(chunk != nullptr);
                for (std::uint8_t id : chunk->blocks) if (id == 4) ++logBlocks;
                ++sampled;
            }
        }
        assert(static_cast<double>(logBlocks) / sampled < 14.0);
        assert(logBlocks > 0);
        std::filesystem::remove_all(forestDirectory, error);
    }

    std::filesystem::remove_all(directory, error);
    return 0;
}
