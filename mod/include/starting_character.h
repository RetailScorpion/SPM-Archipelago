#pragma once

#include <common.h>

/*
    The starting_character option (user, 2026-11-07): starting as Peach, Bowser or Luigi, with Mario an
    item to find (game item 216).

    The client puts the starting character in the mailbox's seedFlags (SEED_START_CHARACTER bits; 0 is
    Mario, the vanilla start). Once a file is past the opening (story value 11) and the seed's items are
    in the game, the mod adds that character to the party, switches to them and takes Mario
    out of the party (his slot emptied, so the character menu doesn't list him; user 2026-11-08: hidden,
    not greyed out), once per file (AP_GSWF_START_CHARACTER_SET). He's added back (pouchAddItem, into
    the first empty slot, so he comes back last in the menu) when his item arrives (AP_GSWF_MARIO_HELD).

    The logic for Mario (which locations need his flip to 3D) is in apworld rules.py, still being mapped.
*/

namespace mod::starting_character {

// Whether Mario can be played: always, unless the seed starts with someone else and his item
// hasn't arrived.
bool marioHeld();

// Mario went into the party some other way (his item found in a level, which the game's own item-get
// gives): he counts as held from now on. Called from the inventory hook (received_items). User,
// 2026-11-16: seed21 put Mario in 1-1's Squiglet Card spot; picked up there, the flag was never set, so
// the next map change took him out again.
void onMarioAdded();

// Mario's item arrived: he's back in the party, with a "Mario has joined your party!" text box.
void giveMario();

// The mod's text for the join message (game_messages), or nullptr.
const char * message(const char * msgName);

// Keeps Mario out of the party on a new map until his item arrives. Call on every map change.
void onMapChange();

// Switches to the starting character once it's due and the player has control (every frame: on a
// fresh file the client places the seed after the first map has loaded). Call every frame.
void tick();

}
