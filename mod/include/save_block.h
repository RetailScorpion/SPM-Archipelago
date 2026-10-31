#pragma once

#include <common.h>

/*
    The Save Block: a key item that saves the game from the key item menu, anywhere.
    (The save_block option shuffles it, gives it at the start, or leaves it out.)

    It takes over item 45. That item's only use in vanilla is as a prop in two 7-3 scenes (a picture
    of Peach shown as a world item, never collected), so those scenes now use item 11 (an unused
    pickup) with item 45's old Peach picture. Item 45 gets the Save Block picture and the mod's name
    and description. Using it runs the save block's own save script, the way the key item menu runs
    the Floro Sprout's and the Return Pipe's scripts.

    In the Pits of 100 Trials (maps starting with dan_) it's greyed out in the menu, the way the
    Return Pipe is where it can't be used, and using it only says it can't be used here.

    The key item menu's code is changed by small "stubs": short lists of instructions the mod writes
    itself, which the game's code jumps to, and which jump back into it.
*/

namespace mod::save_block {

constexpr s32 SAVE_BLOCK_ITEM_ID = 45;

// Sets up item 45 and the key item menu. Call once at startup.
void init();

// Greys the Save Block out in the Pits. Call on every map change.
void onMapChange(const char * nextMap);

// The mod's text for a game message name, or nullptr to use the game's own.
const char * message(const char * msgName);

}
