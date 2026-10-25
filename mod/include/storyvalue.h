#pragma once

#include <common.h>

/*
    Per-chapter story value (GSW 0).

    The game has one story value; the randomizer needs each chapter to keep its own. On every map
    change, the slot of the chapter being entered is loaded into the real GSW 0. Hub maps (Flipside,
    Flopside) always see a fixed value. Maps that belong to neither (prologue/ending maps, pits,
    minigames) keep whatever value is current.

    Writing the real GSW 0 (rather than hooking swByteGet) means native code that reads gp->gsw0
    directly sees the same value as scripts.

    The slots live in reserved GSW bytes (savedata.h), so they're saved with the game. The current
    chapter's slot is synced from GSW 0 every frame, so a save made mid-chapter is up to date.
*/

namespace mod::storyvalue {

#define STORY_CONTEXT_NONE -1  // map isn't in a chapter or a hub: value carries over
#define STORY_CONTEXT_HUB 0    // Flipside / Flopside
// 1-8: chapters

#define STORY_CHAPTER_COUNT 8

// Hub value: after the Pure Heart is placed in the Flipside 1F Outskirts pillar (Peach revived),
// so the Lineland and Gloam Valley doors are both open and no hub cutscene is pending
#define STORY_HUB_VALUE 65

// Story value when the prologue reaches the Lineland door; below it, the story value is left alone
#define STORY_PROLOGUE_END 11

struct StoryState
{
    s32 context;                         // context of the current map
    s32 slots[STORY_CHAPTER_COUNT + 1];  // copy for the client: [0] = hub value, [1-8] = chapters
};

extern StoryState gStory;

void init();

/*
    Forget the current context (on the title screen), so the next map change doesn't save the
    previous file's value into the newly loaded one.
*/
void resetContext();

/*
    Swaps GSW 0 for a map change to nextMap. Call before any of the new map's scripts run.
*/
void onMapChange(const char * nextMap);

/*
    Keeps the current chapter's saved slot up to date. Call every frame in game.
*/
void sync();

s32 contextForMap(const char * map);

// Value a chapter's slot holds (its start value if it hasn't been entered yet)
s32 getChapterValue(s32 chapter);

// Whether a chapter's ending has played
bool isChapterFinished(s32 chapter);

// How many of a chapter's sections (X-1 to X-4) have been reached: 1-4. Chapters 6 and 8 don't
// follow the four-section pattern and only report 1 or (finished) 4.
s32 sectionsReached(s32 chapter);

}
