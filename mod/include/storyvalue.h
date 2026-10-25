#pragma once

#include <common.h>

/*
    Per-chapter story value (GSW 0) experiment.

    The game has one story value; the randomizer needs each chapter to keep its own. On every map
    change, the value of the chapter being left is saved to that chapter's slot, and the slot of the
    chapter being entered is loaded into the real GSW 0. Hub maps (Flipside, Flopside) always see a
    fixed value. Maps that belong to neither (prologue/ending maps, pits, minigames) keep whatever
    value is current.

    Writing the real GSW 0 (rather than hooking swByteGet) means native code that reads gp->gsw0
    directly sees the same value as scripts.

    Slots live in mod memory for now, so they reset when the game restarts.
*/

namespace mod::storyvalue {

#define STORY_CONTEXT_NONE -1  // map isn't in a chapter or a hub: value carries over
#define STORY_CONTEXT_HUB 0    // Flipside / Flopside
// 1-8: chapters

#define STORY_SLOT_COUNT 9

struct StoryState
{
    s32 context;                   // context of the current map
    s32 slots[STORY_SLOT_COUNT];   // [0] = hub value, [1-8] = each chapter's saved value
};

extern StoryState gStory;

void init();

/*
    Swaps GSW 0 for a map change to nextMap. Call before any of the new map's scripts run.
*/
void onMapChange(const char * nextMap);

s32 contextForMap(const char * map);

}
