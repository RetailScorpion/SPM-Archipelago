#pragma once

#include <common.h>

/*
    Key item storage: an overflow for the 32 key item slots.

    The game has 48 key items and room for 32; vanilla never holds them all at once (story keys get
    used up), but a randomizer can. A key item added while all 32 slots are full goes into storage
    instead (AP_GSW_KEY_STORAGE, saved with the game), with a message on screen. Stored items don't
    count as held: SPM Archipelago Tools (client/ap_tools.py) swaps them with carried ones.
*/

namespace mod::keystorage {

#define KEY_STORAGE_SLOTS 32

// All 32 key item slots are taken
bool keyItemsFull();

// Puts a key item in storage (with the on-screen message); false if storage is full too
bool store(s32 itemId);

// Call every frame in game: draws the message while it lasts
void tick();

}
