#pragma once

#include <common.h>

/*
    Save lock (user, 2026-11-19): an Archipelago file belongs to one seed and slot, so loading it while
    the client is connected to another can't send its checks there.

    The client puts a number for its seed and slot in the mailbox (seedId). The first time an
    Archipelago file sees one, it keeps it in the save (AP_GSW_SEED_ID). If the file is ever played
    while the client has a different one, the player gets a text box saying so, and the game goes to
    its game over screen, which leads back to the title. The client checks the same number, and leaves
    a file with another seed's number alone (no items placed, no checks sent). Files from before this
    have no number yet, so the first seed they meet claims them.
*/

namespace mod::save_lock {

// Claims the file for the client's seed, or sends the player off it. Call every frame while playing an
// Archipelago file, before connect_gate.
void tick();

// Whether the file belongs to another seed than the client's (the box is up or coming).
bool mismatched();

// Forgets the box (its script ends with the save). Call while no save is loaded.
void onNoSaveLoaded();

// The text box's message (game_messages), or nullptr.
const char * message(const char * msgName);

}
