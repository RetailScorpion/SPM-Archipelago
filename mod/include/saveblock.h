#pragma once

#include <common.h>

/*
    The Save Block: a key item that saves the game from the key item menu, anywhere (the save_block
    option shuffles it, gives it at the start or leaves it out).

    It takes item 45 (STG7_PEACH), whose only use is a prop in two 7-3 scenes (Peach shown as a world
    item, never collected): those now spawn item 11 (an unused pickup) given item 45's Peach icon.
    Item 45 gets the save block icon (built from the save block's own textures, wicon id 405) and the
    mod's name and description. Using it runs the save block's save (evt_memcard_save), as the key
    item menu runs the Floro Sprout's and the Return Pipe's scripts.

    In the Pits of 100 Trials (dan_ maps) it's greyed out in the menu, the way the Return Pipe is where
    it can't be used, and using it only says it can't be used here.
*/

namespace mod::saveblock {

constexpr s32 SAVE_BLOCK_ITEM_ID = 45;

void init();

// Call on every map change
void onMapChange(const char * nextMap);

// The mod's text for a message, or nullptr to use the game's
const char * message(const char * msgName);

}
