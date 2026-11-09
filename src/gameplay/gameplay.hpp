#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mc {

constexpr std::size_t InventorySize = 32;
constexpr std::size_t HotbarSize = 8;
constexpr int StackLimit = 64;

struct ItemStack {
    int itemId = 0;
    int count = 0;
};

class Inventory {
public:
    int addItem(int itemId, int count);
    bool removeItem(int itemId, int count);
    int countItem(int itemId) const;
    ItemStack& slot(std::size_t index);
    const ItemStack& slot(std::size_t index) const;
    const std::array<ItemStack, InventorySize>& slots() const;
    void replace(const std::array<ItemStack, InventorySize>& slots);
    void clear();

private:
    std::array<ItemStack, InventorySize> slots_{};
};

struct Recipe {
    int inputId;
    int inputCount;
    int outputId;
    int outputCount;
    const char* name;
};

class Crafting {
public:
    static std::size_t recipeCount();
    static const Recipe& recipe(std::size_t index);
    static bool canCraft(const Inventory& inventory, std::size_t index);
    static bool craft(Inventory& inventory, std::size_t index);
    static int craftFirstAvailable(Inventory& inventory);
};

struct ItemDrop {
    int itemId = 0;
    int count = 1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float age = 0.0f;
};

}
