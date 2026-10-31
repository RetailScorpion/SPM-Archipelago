#pragma once

#include <common.h>

/*
    Key item storage: extra room for key items.

    Mario can carry 32 key items, but the game has 48. Vanilla never needs room for them all at
    once, since story keys get used up along the way; a randomizer can hand them out in any order.
    So when a key item arrives while all 32 slots are full, it goes into storage instead (32 more
    slots, kept in the save file), and a red notice says so on screen.

    Stored items don't count as carried. SPM Archipelago Tools (client/ap_tools.py) swaps them with
    carried ones.
*/

namespace mod::key_item_storage {

#define KEY_STORAGE_SLOTS 32

// Whether all 32 key item slots are taken.
bool keyItemsFull();

// Puts a key item in storage and shows the notice. Returns false if storage is full too.
bool store(s32 itemId);

// Draws the notice while it lasts. Call every frame in game.
void tick();

}
