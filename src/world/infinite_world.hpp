#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mc::world {

constexpr int ChunkSize = 16;
constexpr int WorldHeight = 64;
constexpr std::size_t ChunkVolume = static_cast<std::size_t>(ChunkSize) * WorldHeight * ChunkSize;

struct ChunkPos {
    int x = 0;
    int z = 0;
    bool operator==(const ChunkPos& other) const noexcept {
        return x == other.x && z == other.z;
    }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& p) const noexcept {
        const std::uint64_t x = static_cast<std::uint32_t>(p.x);
        const std::uint64_t z = static_cast<std::uint32_t>(p.z);
        std::uint64_t value = (x << 32) | z;
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 27;
        value *= 0x94d049bb133111ebULL;
        value ^= value >> 31;
        return static_cast<std::size_t>(value);
    }
};

inline int floorDiv(int value, int divisor = ChunkSize) noexcept {
    const int quotient = value / divisor;
    const int remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

inline int positiveMod(int value, int divisor = ChunkSize) noexcept {
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

inline std::uint32_t hash(int x, int z, std::uint32_t seed) noexcept {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u
        + static_cast<std::uint32_t>(z) * 668265263u + seed * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return n ^ (n >> 16);
}

inline int terrainHeight(int x, int z) noexcept {
    const double broad = std::sin(x * 0.0125) * 8.0 + std::cos(z * 0.014) * 7.0;
    const double detail = std::sin((x + z) * 0.037) * 2.2
        + std::cos((x - z) * 0.031) * 1.8;
    const double hills = std::sin(x * 0.0031 + z * 0.0027) * 9.0;
    return std::clamp(25 + static_cast<int>(std::round(broad + detail + hills)), 5, WorldHeight - 8);
}

inline int biomeAt(int x, int z) noexcept {
    const float temperature = std::sin(x * 0.0041f) + std::cos(z * 0.0037f);
    const float moisture = std::sin((x + z) * 0.0048f) + std::cos((x - z) * 0.0033f);
    if (temperature > 0.8f && moisture < -0.1f) return 1;
    if (temperature < -0.72f) return 2;
    if (moisture > 0.62f) return 3;
    return 0;
}

struct Chunk {
    ChunkPos position;
    std::array<std::uint8_t, ChunkVolume> blocks{};
    bool modified = false;
    bool meshDirty = true;

    static constexpr std::size_t index(int x, int y, int z) noexcept {
        return (static_cast<std::size_t>(x) * WorldHeight + static_cast<std::size_t>(y))
            * ChunkSize + static_cast<std::size_t>(z);
    }

    std::uint8_t get(int x, int y, int z) const noexcept {
        if (x < 0 || x >= ChunkSize || z < 0 || z >= ChunkSize || y < 0 || y >= WorldHeight)
            return 0;
        return blocks[index(x, y, z)];
    }

    void set(int x, int y, int z, std::uint8_t id) noexcept {
        if (x < 0 || x >= ChunkSize || z < 0 || z >= ChunkSize || y < 0 || y >= WorldHeight)
            return;
        auto& block = blocks[index(x, y, z)];
        if (block == id) return;
        block = id;
        modified = true;
        meshDirty = true;
    }
};

class InfiniteWorld {
public:
    explicit InfiniteWorld(std::filesystem::path saveDirectory = "worlds/default/chunks")
        : saveDirectory_(std::move(saveDirectory)) {}

    static ChunkPos chunkOf(int worldX, int worldZ) noexcept {
        return {floorDiv(worldX), floorDiv(worldZ)};
    }

    static int localOf(int coordinate) noexcept {
        return positiveMod(coordinate);
    }

    static int heightAt(int worldX, int worldZ) noexcept {
        return terrainHeight(worldX, worldZ);
    }

    std::uint8_t getBlock(int worldX, int y, int worldZ) {
        if (y < 0) return 3;
        if (y >= WorldHeight) return 0;
        Chunk& chunk = getOrCreate(chunkOf(worldX, worldZ));
        return chunk.get(localOf(worldX), y, localOf(worldZ));
    }

    bool setBlock(int worldX, int y, int worldZ, std::uint8_t id) {
        if (y < 0 || y >= WorldHeight) return false;
        const ChunkPos pos = chunkOf(worldX, worldZ);
        Chunk& chunk = getOrCreate(pos);
        const auto before = chunk.get(localOf(worldX), y, localOf(worldZ));
        if (before == id) return false;
        chunk.set(localOf(worldX), y, localOf(worldZ), id);
        if (localOf(worldX) == 0) markNeighborDirty({pos.x - 1, pos.z});
        if (localOf(worldX) == ChunkSize - 1) markNeighborDirty({pos.x + 1, pos.z});
        if (localOf(worldZ) == 0) markNeighborDirty({pos.x, pos.z - 1});
        if (localOf(worldZ) == ChunkSize - 1) markNeighborDirty({pos.x, pos.z + 1});
        return true;
    }

    void updateStreaming(int centerX, int centerZ, int renderDistance) {
        renderDistance = std::clamp(renderDistance, 2, 32);
        const ChunkPos center = chunkOf(centerX, centerZ);
        for (int dz = -renderDistance; dz <= renderDistance; ++dz) {
            for (int dx = -renderDistance; dx <= renderDistance; ++dx) {
                if (dx * dx + dz * dz > renderDistance * renderDistance) continue;
                getOrCreate({center.x + dx, center.z + dz});
            }
        }
        const int unloadDistance = renderDistance + 2;
        for (auto it = chunks_.begin(); it != chunks_.end();) {
            const int dx = it->first.x - center.x;
            const int dz = it->first.z - center.z;
            if (dx * dx + dz * dz > unloadDistance * unloadDistance) {
                saveChunk(it->second);
                it = chunks_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::vector<ChunkPos> visibleChunks(int centerX, int centerZ, int renderDistance) const {
        const ChunkPos center = chunkOf(centerX, centerZ);
        renderDistance = std::clamp(renderDistance, 2, 32);
        std::vector<ChunkPos> result;
        for (const auto& entry : chunks_) {
            const int dx = entry.first.x - center.x;
            const int dz = entry.first.z - center.z;
            if (dx * dx + dz * dz <= renderDistance * renderDistance)
                result.push_back(entry.first);
        }
        std::sort(result.begin(), result.end(), [](const ChunkPos& a, const ChunkPos& b) {
            return a.z == b.z ? a.x < b.x : a.z < b.z;
        });
        return result;
    }

    bool saveAll() {
        bool success = true;
        for (auto& entry : chunks_) success = saveChunk(entry.second) && success;
        return success;
    }

    std::size_t loadedCount() const noexcept {
        return chunks_.size();
    }

    bool isLoaded(ChunkPos position) const noexcept {
        return chunks_.find(position) != chunks_.end();
    }

    bool meshDirty(ChunkPos position) const noexcept {
        const auto found = chunks_.find(position);
        return found != chunks_.end() && found->second.meshDirty;
    }

    void markMeshClean(ChunkPos position) noexcept {
        const auto found = chunks_.find(position);
        if (found != chunks_.end()) found->second.meshDirty = false;
    }

    std::vector<ChunkPos> loadedChunkPositions() const {
        std::vector<ChunkPos> result;
        result.reserve(chunks_.size());
        for (const auto& entry : chunks_) result.push_back(entry.first);
        std::sort(result.begin(), result.end(), [](const ChunkPos& a, const ChunkPos& b) {
            return a.z == b.z ? a.x < b.x : a.z < b.z;
        });
        return result;
    }

private:
    std::filesystem::path saveDirectory_;
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks_;

    static std::uint8_t generatedBlock(int x, int y, int z) noexcept {
        if (y < 0) return 3;
        if (y >= WorldHeight) return 0;
        const int top = terrainHeight(x, z);
        if (y > top) return 0;
        const int biome = biomeAt(x, z);
        if (y == top) return biome == 1 ? 6 : (biome == 2 ? 7 : 1);
        if (y >= top - 4) return biome == 1 ? 6 : (biome == 2 ? 7 : 2);
        const float caveA = std::sin(x * 0.071f + y * 0.113f)
            * std::cos(z * 0.063f - y * 0.087f);
        const float caveB = std::sin((x + z) * 0.039f + y * 0.097f);
        if (y > 3 && y < top - 4 && caveA + caveB > 1.12f) return 0;
        if (y < top - 6 && hash(x * 3 + z, y * 7, 413) % 181u == 0u) return 8;
        return 3;
    }

    static Chunk generate(ChunkPos position) {
        Chunk chunk;
        chunk.position = position;
        const int originX = position.x * ChunkSize;
        const int originZ = position.z * ChunkSize;
        for (int x = 0; x < ChunkSize; ++x) {
            for (int z = 0; z < ChunkSize; ++z) {
                const int wx = originX + x;
                const int wz = originZ + z;
                const int top = terrainHeight(wx, wz);
                for (int y = 0; y <= top; ++y)
                    chunk.blocks[Chunk::index(x, y, z)] = generatedBlock(wx, y, wz);
            }
        }

        for (int anchorX = originX - 2; anchorX < originX + ChunkSize + 2; ++anchorX) {
            for (int anchorZ = originZ - 2; anchorZ < originZ + ChunkSize + 2; ++anchorZ) {
                const int biome = biomeAt(anchorX, anchorZ);
                const std::uint32_t roll = hash(anchorX, anchorZ, 77) % 100u;
                if (biome == 1 || roll > (biome == 3 ? 19u : 8u)) continue;
                const int ground = terrainHeight(anchorX, anchorZ);
                if (generatedBlock(anchorX, ground, anchorZ) != 1) continue;
                const int trunk = biome == 3 ? 5 : 4;
                for (int dy = 1; dy <= trunk; ++dy) {
                    const int wx = anchorX;
                    const int wz = anchorZ;
                    if (wx >= originX && wx < originX + ChunkSize &&
                        wz >= originZ && wz < originZ + ChunkSize)
                        chunk.set(wx - originX, ground + dy, wz - originZ, 4);
                }
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dz = -2; dz <= 2; ++dz) {
                        for (int dy = trunk - 1; dy <= trunk + 2; ++dy) {
                            const int wx = anchorX + dx;
                            const int wz = anchorZ + dz;
                            const int wy = ground + dy;
                            if (wx < originX || wx >= originX + ChunkSize ||
                                wz < originZ || wz >= originZ + ChunkSize ||
                                wy < 0 || wy >= WorldHeight) continue;
                            if (std::abs(dx) + std::abs(dz) + std::abs(dy - (trunk + 1)) > 4) continue;
                            if (chunk.get(wx - originX, wy, wz - originZ) == 0)
                                chunk.set(wx - originX, wy, wz - originZ, 5);
                        }
                    }
                }
            }
        }
        chunk.modified = false;
        chunk.meshDirty = true;
        return chunk;
    }

    Chunk& getOrCreate(ChunkPos position) {
        auto found = chunks_.find(position);
        if (found != chunks_.end()) return found->second;
        Chunk chunk;
        if (!loadChunk(position, chunk)) chunk = generate(position);
        return chunks_.emplace(position, std::move(chunk)).first->second;
    }

    void markNeighborDirty(ChunkPos position) {
        auto found = chunks_.find(position);
        if (found != chunks_.end()) found->second.meshDirty = true;
    }

    std::filesystem::path chunkPath(ChunkPos position) const {
        return saveDirectory_ / (std::to_string(position.x) + "_" +
            std::to_string(position.z) + ".chunk");
    }

    bool loadChunk(ChunkPos position, Chunk& chunk) const {
        std::ifstream file(chunkPath(position), std::ios::binary);
        if (!file) return false;
        struct Header { char magic[8]; std::int32_t x; std::int32_t z; };
        Header header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        const char expected[8] = {'M','C','3','D','C','H','1','\0'};
        if (!file || !std::equal(std::begin(expected), std::end(expected), std::begin(header.magic)) ||
            header.x != position.x || header.z != position.z) return false;
        file.read(reinterpret_cast<char*>(chunk.blocks.data()),
            static_cast<std::streamsize>(chunk.blocks.size()));
        if (!file) return false;
        chunk.position = position;
        chunk.modified = false;
        chunk.meshDirty = true;
        return true;
    }

    bool saveChunk(const Chunk& chunk) const {
        if (!chunk.modified) return true;
        std::error_code error;
        std::filesystem::create_directories(saveDirectory_, error);
        if (error) return false;
        const auto path = chunkPath(chunk.position);
        const auto temporary = path.string() + ".tmp";
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        struct Header { char magic[8]; std::int32_t x; std::int32_t z; };
        Header header{{'M','C','3','D','C','H','1','\0'}, chunk.position.x, chunk.position.z};
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(chunk.blocks.data()),
            static_cast<std::streamsize>(chunk.blocks.size()));
        file.close();
        if (!file) return false;
        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::filesystem::remove(path, error);
            error.clear();
            std::filesystem::rename(temporary, path, error);
        }
        return !error;
    }
};

}
