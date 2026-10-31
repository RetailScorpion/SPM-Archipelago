#pragma once

#include <common.h>

/*
    Two characters whose behaviour depends on the story value, changed to depend on what Mario has.

    Watchitt (Yold Town, he2_03): in vanilla he sends for the bridge builder once Chapter 1 reaches
    25 (Thoreau's join scene has played), and says "go get help" before that. Now he sends for the
    builder as soon as Mario has Thoreau, whatever the story value.

    Bestovius (1-1, he1_06): in vanilla he teaches the flip below Chapter 1's story value 15 and only
    chats from 16 on. Now his lesson is a location (story_gifts): he offers it until it's collected,
    whatever Chapter 1's value is. Finishing it still moves Chapter 1 on to 16, as in vanilla. (Being
    able to flip follows the flip option, flip_ability, not the lesson.)
*/

namespace mod::npc_story_fixes {

// Patches Watchitt's and Bestovius's scripts. Call once at startup.
void init();

// Updates Watchitt's save byte from whether Mario has Thoreau. Call every frame in game.
void tick();

// Updates Bestovius's save byte from whether the lesson is collected. Call on every map change.
void onMapChange();

}
