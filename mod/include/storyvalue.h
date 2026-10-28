#pragma once

#include <common.h>
#include <spm/nandmgr.h>

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

    Slots only move forwards. A scene played out of order (a Pixl join after the chapter has moved
    on) still sets its original story value; that lower value is ignored and the chapter's own
    value comes back at the next map change.
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

// The same for a save file that isn't loaded (the file select)
bool isChapterFinishedInSave(const spm::nandmgr::SaveFile * save, s32 chapter);

/*
    Moves a chapter's story value forward to value (never backwards), as if that chapter's own scene
    had set it. For story steps vanilla plays in the hub, whose writes the mod discards.
*/
void advanceChapter(s32 chapter, s32 value);

// How many of a chapter's sections (X-1 to X-4) have been reached: 1-4. Chapters 6 and 8 don't
// follow the four-section pattern and only report 1 or (finished) 4.
s32 sectionsReached(s32 chapter);

}
