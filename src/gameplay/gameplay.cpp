#include "gameplay.hpp"

#include <algorithm>

namespace mc {

int Inventory::addItem(int itemId, int count) {
    if (itemId <= 0 || count <= 0) return count;
    for (ItemStack& stack : slots_) {
        if (stack.itemId != itemId || stack.count >= StackLimit) continue;
        int moved = std::min(count, StackLimit - stack.count);
        stack.count += moved;
        count -= moved;
        if (count == 0) return 0;
    }
    for (ItemStack& stack : slots_) {
        if (stack.itemId != 0) continue;
        int moved = std::min(count, StackLimit);
        stack.itemId = itemId;
        stack.count = moved;
        count -= moved;
        if (count == 0) return 0;
    }
    return count;
}

bool Inventory::removeItem(int itemId, int count) {
    if (itemId <= 0 || count <= 0 || countItem(itemId) < count) return false;
    for (ItemStack& stack : slots_) {
        if (stack.itemId != itemId) continue;
        int removed = std::min(count, stack.count);
        stack.count -= removed;
        count -= removed;
        if (stack.count == 0) stack.itemId = 0;
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

void Inventory::replace(const std::array<ItemStack, InventorySize>& slots) {
    slots_ = slots;
    for (ItemStack& stack : slots_) {
        if (stack.itemId <= 0 || stack.count <= 0) stack = {};
        else if (stack.count > StackLimit) stack.count = StackLimit;
    }
}

void Inventory::clear() {
    slots_.fill({});
}

namespace {
constexpr Recipe Recipes[] = {
    {4, 1, 9, 4, "Wooden planks"},
    {9, 2, 10, 4, "Sticks"},
    {9, 4, 11, 1, "Crafting table"}
};
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
    return inventory.countItem(selected.inputId) >= selected.inputCount;
}

bool Crafting::craft(Inventory& inventory, std::size_t index) {
    if (!canCraft(inventory, index)) return false;
    const Recipe& selected = Recipes[index];
    Inventory result = inventory;
    if (result.addItem(selected.outputId, selected.outputCount) != 0) return false;
    if (!result.removeItem(selected.inputId, selected.inputCount)) return false;
    inventory = result;
    return true;
}

int Crafting::craftFirstAvailable(Inventory& inventory) {
    for (std::size_t i = 0; i < recipeCount(); ++i)
        if (craft(inventory, i)) return static_cast<int>(i);
    return -1;
}

}
