#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mc {

constexpr std::size_t InventorySize = 32;
constexpr std::size_t HotbarSize = 8;
constexpr int StackLimit = 64;

enum class ToolType : uint8_t {
    None,
    Pickaxe,
    Axe,
    Shovel,
    Sword
};

struct ItemStack {
    int itemId = 0;
    int count = 0;
};

struct Ingredient {
    int itemId = 0;
    int count = 0;
};

struct Recipe {
    std::array<Ingredient, 3> ingredients{};
    std::size_t ingredientCount = 0;
    int outputId = 0;
    int outputCount = 0;
    const char* name = "";
};

struct ItemDefinition {
    int id;
    const char* name;
    ToolType tool;
    int maxDurability;
    float efficiency;
};

class Items {
public:
    static const ItemDefinition* find(int itemId);
    static ToolType toolType(int itemId);
    static int maxDurability(int itemId);
    static float miningEfficiency(int itemId, int blockId);
    static bool canHarvest(int itemId, int blockId);
    static bool isTool(int itemId);
};

class Inventory {
public:
    int addItem(int itemId, int count);
    bool removeItem(int itemId, int count);
    int countItem(int itemId) const;
    ItemStack& slot(std::size_t index);
    const ItemStack& slot(std::size_t index) const;
    const std::array<ItemStack, InventorySize>& slots() const;
    const std::array<int, InventorySize>& durabilitySlots() const;
    int durability(std::size_t index) const;
    bool damageItem(std::size_t index, int amount = 1);
    void swapSlots(std::size_t first, std::size_t second);
    void replace(const std::array<ItemStack, InventorySize>& slots);
    void replace(const std::array<ItemStack, InventorySize>& slots,
                 const std::array<int, InventorySize>& durability);
    void clear();

private:
    std::array<ItemStack, InventorySize> slots_{};
    std::array<int, InventorySize> durability_{};
};

class Crafting {
public:
    static std::size_t recipeCount();
    static const Recipe& recipe(std::size_t index);
    static bool canCraft(const Inventory& inventory, std::size_t index);
    static bool craft(Inventory& inventory, std::size_t index);
    static int craftFirstAvailable(Inventory& inventory);
};

class PlayerStats {
public:
    static constexpr float MaxHealth = 20.0f;
    static constexpr float MaxHunger = 20.0f;

    float health() const;
    float hunger() const;
    void set(float health, float hunger);
    void damage(float amount);
    void heal(float amount);
    void tick(float dt, bool moving, bool sprinting);
    bool alive() const;

private:
    float health_ = MaxHealth;
    float hunger_ = MaxHunger;
    float exhaustionTimer_ = 0.0f;
    float regenerationTimer_ = 0.0f;
    float starvationTimer_ = 0.0f;
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
