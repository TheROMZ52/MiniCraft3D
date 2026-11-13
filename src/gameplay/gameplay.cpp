#include "gameplay.hpp"

#include <algorithm>
#include <cmath>

namespace mc {

namespace {
constexpr ItemDefinition Definitions[] = {
    {20, "Wooden Pickaxe", ToolType::Pickaxe, 59, 2.0f},
    {21, "Stone Pickaxe", ToolType::Pickaxe, 131, 4.0f},
    {22, "Wooden Axe", ToolType::Axe, 59, 2.0f},
    {23, "Wooden Shovel", ToolType::Shovel, 59, 2.0f},
    {24, "Wooden Sword", ToolType::Sword, 59, 1.0f},
    {25, "Stone Axe", ToolType::Axe, 131, 5.0f},
    {26, "Stone Shovel", ToolType::Shovel, 131, 4.0f},
    {27, "Stone Sword", ToolType::Sword, 131, 1.5f}
};

constexpr Recipe Recipes[] = {
    {{{{4, 1}, {}, {}}}, 1, 9, 4, "Wooden planks"},
    {{{{9, 2}, {}, {}}}, 1, 10, 4, "Sticks"},
    {{{{9, 4}, {}, {}}}, 1, 11, 1, "Crafting table"},
    {{{{9, 3}, {10, 2}, {}}}, 2, 20, 1, "Wooden pickaxe"},
    {{{{3, 3}, {10, 2}, {}}}, 2, 21, 1, "Stone pickaxe"},
    {{{{9, 3}, {10, 2}, {}}}, 2, 22, 1, "Wooden axe"},
    {{{{9, 1}, {10, 2}, {}}}, 2, 23, 1, "Wooden shovel"},
    {{{{9, 2}, {10, 1}, {}}}, 2, 24, 1, "Wooden sword"},
    {{{{3, 3}, {10, 2}, {}}}, 2, 25, 1, "Stone axe"},
    {{{{3, 1}, {10, 2}, {}}}, 2, 26, 1, "Stone shovel"},
    {{{{3, 2}, {10, 1}, {}}}, 2, 27, 1, "Stone sword"}
};
}

const ItemDefinition* Items::find(int itemId) {
    for (const ItemDefinition& item : Definitions)
        if (item.id == itemId) return &item;
    return nullptr;
}

ToolType Items::toolType(int itemId) {
    const ItemDefinition* item = find(itemId);
    return item ? item->tool : ToolType::None;
}

int Items::maxDurability(int itemId) {
    const ItemDefinition* item = find(itemId);
    return item ? item->maxDurability : 0;
}

bool Items::isTool(int itemId) {
    return find(itemId) != nullptr;
}

bool Items::canHarvest(int itemId, int blockId) {
    ToolType tool = toolType(itemId);
    if (blockId == 3) return tool == ToolType::Pickaxe;
    if (blockId == 8) return itemId == 21;
    if (blockId == 4 || blockId == 5 || blockId == 9) return tool == ToolType::Axe;
    if (blockId == 1 || blockId == 2 || blockId == 6 || blockId == 7)
        return tool == ToolType::Shovel;
    return false;
}

float Items::miningEfficiency(int itemId, int blockId) {
    const ItemDefinition* item = find(itemId);
    if (!item) return 1.0f;
    if (canHarvest(itemId, blockId)) return item->efficiency;
    return 0.65f;
}

int Inventory::addItem(int itemId, int count) {
    if (itemId <= 0 || count <= 0) return count;
    const bool tool = Items::isTool(itemId);
    const int maxStack = tool ? 1 : StackLimit;
    if (!tool) {
        for (ItemStack& stack : slots_) {
            if (stack.itemId != itemId || stack.count >= maxStack) continue;
            int moved = std::min(count, maxStack - stack.count);
            stack.count += moved;
            count -= moved;
            if (count == 0) return 0;
        }
    }
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        ItemStack& stack = slots_[i];
        if (stack.itemId != 0) continue;
        int moved = std::min(count, maxStack);
        stack.itemId = itemId;
        stack.count = moved;
        durability_[i] = tool ? Items::maxDurability(itemId) : 0;
        count -= moved;
        if (count == 0) return 0;
    }
    return count;
}

bool Inventory::removeItem(int itemId, int count) {
    if (itemId <= 0 || count <= 0 || countItem(itemId) < count) return false;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        ItemStack& stack = slots_[i];
        if (stack.itemId != itemId) continue;
        int removed = std::min(count, stack.count);
        stack.count -= removed;
        count -= removed;
        if (stack.count == 0) {
            stack = {};
            durability_[i] = 0;
        }
        if (count == 0) return true;
    }
    return true;
}

int Inventory::countItem(int itemId) const {
    int total = 0;
    for (const ItemStack& stack : slots_)
        if (stack.itemId == itemId) total += stack.count;
    return total;
}

ItemStack& Inventory::slot(std::size_t index) {
    return slots_.at(index);
}

const ItemStack& Inventory::slot(std::size_t index) const {
    return slots_.at(index);
}

const std::array<ItemStack, InventorySize>& Inventory::slots() const {
    return slots_;
}

const std::array<int, InventorySize>& Inventory::durabilitySlots() const {
    return durability_;
}

int Inventory::durability(std::size_t index) const {
    return durability_.at(index);
}

bool Inventory::damageItem(std::size_t index, int amount) {
    if (index >= slots_.size() || amount <= 0) return false;
    ItemStack& stack = slots_[index];
    if (stack.itemId <= 0 || !Items::isTool(stack.itemId)) return false;
    durability_[index] = std::max(0, durability_[index] - amount);
    if (durability_[index] == 0) {
        stack = {};
        return true;
    }
    return false;
}

void Inventory::swapSlots(std::size_t first, std::size_t second) {
    if (first >= slots_.size() || second >= slots_.size()) return;
    std::swap(slots_[first], slots_[second]);
    std::swap(durability_[first], durability_[second]);
}

void Inventory::replace(const std::array<ItemStack, InventorySize>& slots) {
    std::array<int, InventorySize> durability{};
    for (std::size_t i = 0; i < slots.size(); ++i)
        durability[i] = Items::maxDurability(slots[i].itemId);
    replace(slots, durability);
}

void Inventory::replace(const std::array<ItemStack, InventorySize>& slots,
                        const std::array<int, InventorySize>& durability) {
    slots_ = slots;
    durability_ = durability;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        ItemStack& stack = slots_[i];
        if (stack.itemId <= 0 || stack.count <= 0) {
            stack = {};
            durability_[i] = 0;
        } else {
            int limit = Items::isTool(stack.itemId) ? 1 : StackLimit;
            stack.count = std::clamp(stack.count, 1, limit);
            int maximum = Items::maxDurability(stack.itemId);
            if (maximum == 0) durability_[i] = 0;
            else durability_[i] = std::clamp(durability_[i], 1, maximum);
        }
    }
}

void Inventory::clear() {
    slots_.fill({});
    durability_.fill(0);
}

std::size_t Crafting::recipeCount() {
    return sizeof(Recipes) / sizeof(Recipes[0]);
}

const Recipe& Crafting::recipe(std::size_t index) {
    return Recipes[index];
}

bool Crafting::canCraft(const Inventory& inventory, std::size_t index) {
    if (index >= recipeCount()) return false;
    const Recipe& selected = Recipes[index];
    for (std::size_t i = 0; i < selected.ingredientCount; ++i) {
        const Ingredient& ingredient = selected.ingredients[i];
        if (inventory.countItem(ingredient.itemId) < ingredient.count) return false;
    }
    return true;
}

bool Crafting::craft(Inventory& inventory, std::size_t index) {
    if (!canCraft(inventory, index)) return false;
    const Recipe& selected = Recipes[index];
    Inventory result = inventory;
    if (result.addItem(selected.outputId, selected.outputCount) != 0) return false;
    for (std::size_t i = 0; i < selected.ingredientCount; ++i) {
        const Ingredient& ingredient = selected.ingredients[i];
        if (!result.removeItem(ingredient.itemId, ingredient.count)) return false;
    }
    inventory = result;
    return true;
}

int Crafting::craftFirstAvailable(Inventory& inventory) {
    for (std::size_t i = 0; i < recipeCount(); ++i)
        if (craft(inventory, i)) return static_cast<int>(i);
    return -1;
}

float PlayerStats::health() const {
    return health_;
}

float PlayerStats::hunger() const {
    return hunger_;
}

void PlayerStats::set(float health, float hunger) {
    health_ = std::clamp(health, 0.0f, MaxHealth);
    hunger_ = std::clamp(hunger, 0.0f, MaxHunger);
    exhaustionTimer_ = regenerationTimer_ = starvationTimer_ = 0.0f;
}

void PlayerStats::damage(float amount) {
    if (amount > 0.0f) health_ = std::max(0.0f, health_ - amount);
}

void PlayerStats::heal(float amount) {
    if (amount > 0.0f) health_ = std::min(MaxHealth, health_ + amount);
}

void PlayerStats::tick(float dt, bool moving, bool sprinting) {
    if (dt <= 0.0f) return;
    if (moving) exhaustionTimer_ += dt * (sprinting ? 1.8f : 0.35f);
    if (exhaustionTimer_ >= 1.0f) {
        float spent = std::floor(exhaustionTimer_);
        hunger_ = std::max(0.0f, hunger_ - spent * 0.025f);
        exhaustionTimer_ -= spent;
    }
    if (hunger_ >= 18.0f && health_ < MaxHealth) {
        regenerationTimer_ += dt;
        if (regenerationTimer_ >= 4.0f) {
            heal(1.0f);
            hunger_ = std::max(0.0f, hunger_ - 0.5f);
            regenerationTimer_ = 0.0f;
        }
    } else {
        regenerationTimer_ = 0.0f;
    }
    if (hunger_ <= 0.0f && health_ > 1.0f) {
        starvationTimer_ += dt;
        if (starvationTimer_ >= 4.0f) {
            damage(1.0f);
            starvationTimer_ = 0.0f;
        }
    } else {
        starvationTimer_ = 0.0f;
    }
}

bool PlayerStats::alive() const {
    return health_ > 0.0f;
}

}
