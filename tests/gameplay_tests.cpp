#include "../src/gameplay/gameplay.hpp"

#include <cassert>

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
    assert(mc::Crafting::craft(inventory, 2) == false);
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
    return 0;
}
