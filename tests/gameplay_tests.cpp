#include "../src/gameplay/gameplay.hpp"

#include <cassert>
#include <cmath>

int main() {
    mc::Inventory inventory;
    assert(inventory.addItem(4, 70) == 0);
    assert(inventory.countItem(4) == 70);
    assert(inventory.slot(0).count == 64);
    assert(inventory.slot(1).count == 6);
    assert(inventory.removeItem(4, 65));
    assert(inventory.countItem(4) == 5);
    assert(!inventory.removeItem(4, 6));

    inventory.clear();
    assert(inventory.addItem(4, 1) == 0);
    assert(mc::Crafting::craft(inventory, 0));
    assert(inventory.countItem(4) == 0);
    assert(inventory.countItem(9) == 4);
    assert(mc::Crafting::craft(inventory, 1));
    assert(inventory.countItem(9) == 2);
    assert(inventory.countItem(10) == 4);
    assert(!mc::Crafting::craft(inventory, 2));
    assert(inventory.countItem(9) == 2);
    assert(inventory.addItem(9, 2) == 0);
    assert(mc::Crafting::craft(inventory, 2));
    assert(inventory.countItem(11) == 1);

    mc::Inventory full;
    assert(full.addItem(4, 1) == 0);
    for (int id = 100; id < 131; ++id) assert(full.addItem(id, 64) == 0);
    assert(!mc::Crafting::craft(full, 0));
    assert(full.countItem(4) == 1);
    assert(full.countItem(9) == 0);

    mc::Inventory tools;
    assert(tools.addItem(20, 1) == 0);
    assert(tools.durability(0) == 59);
    assert(tools.addItem(20, 1) == 0);
    assert(tools.slot(1).count == 1);
    assert(tools.damageItem(0, 58) == false);
    assert(tools.durability(0) == 1);
    assert(tools.damageItem(0, 1));
    assert(tools.slot(0).itemId == 0);
    assert(tools.durability(0) == 0);
    assert(mc::Items::canHarvest(20, 3));
    assert(mc::Items::canHarvest(21, 3));
    assert(!mc::Items::canHarvest(20, 8));
    assert(mc::Items::canHarvest(21, 8));
    assert(mc::Items::canHarvest(22, 9));
    assert(mc::Items::canHarvest(23, 6));
    assert(!mc::Items::canHarvest(23, 9));
    assert(mc::Items::miningEfficiency(21, 3) > mc::Items::miningEfficiency(20, 3));
    tools.swapSlots(0, 1);
    assert(tools.slot(0).itemId == 20);
    assert(tools.durability(0) == 59);

    mc::Inventory craftedTools;
    assert(craftedTools.addItem(9, 3) == 0);
    assert(craftedTools.addItem(10, 2) == 0);
    assert(mc::Crafting::craft(craftedTools, 3));
    assert(craftedTools.countItem(20) == 1);
    int toolSlot = -1;
    for (std::size_t i = 0; i < mc::InventorySize; ++i)
        if (craftedTools.slot(i).itemId == 20) toolSlot = static_cast<int>(i);
    assert(toolSlot >= 0);
    assert(craftedTools.durability(static_cast<std::size_t>(toolSlot)) == 59);

    mc::Inventory stoneAxe;
    assert(stoneAxe.addItem(3, 3) == 0);
    assert(stoneAxe.addItem(10, 2) == 0);
    assert(mc::Crafting::craft(stoneAxe, 8));
    assert(stoneAxe.countItem(25) == 1);
    assert(mc::Items::canHarvest(25, 4));
    assert(mc::Items::maxDurability(25) == 131);

    mc::Inventory stoneShovel;
    assert(stoneShovel.addItem(3, 1) == 0);
    assert(stoneShovel.addItem(10, 2) == 0);
    assert(mc::Crafting::craft(stoneShovel, 9));
    assert(stoneShovel.countItem(26) == 1);
    assert(mc::Items::canHarvest(26, 6));

    mc::Inventory stoneSword;
    assert(stoneSword.addItem(3, 2) == 0);
    assert(stoneSword.addItem(10, 1) == 0);
    assert(mc::Crafting::craft(stoneSword, 10));
    assert(stoneSword.countItem(27) == 1);
    assert(mc::Items::toolType(27) == mc::ToolType::Sword);

    mc::PlayerStats stats;
    stats.damage(6.0f);
    assert(stats.health() == 14.0f);
    stats.set(14.0f, 20.0f);
    stats.tick(4.1f, false, false);
    assert(stats.health() == 15.0f);
    assert(stats.hunger() < 20.0f);
    stats.set(20.0f, 0.0f);
    stats.tick(4.1f, false, false);
    assert(stats.health() == 19.0f);
    stats.damage(100.0f);
    assert(!stats.alive());
    return 0;
}
