#pragma once

#include <common.h>

/*
    Each chapter's own story progress.

    The game tracks the whole story with one number, the "story value" (save byte GSW 0): it goes up
    from 0 at the start of the game to 419 at the end, and almost every scene checks it. That only
    works if the chapters are played in order. The randomizer lets you play them in any order, so it
    keeps a separate story value for each chapter:
      - each chapter's value is saved in its own "slot" (2 reserved save bytes, save_data.h)
      - on every map change, the slot of the chapter being entered is loaded into the real story
        value, so that chapter's scenes see their own progress
      - the hubs (Flipside and Flopside) always get a fixed value (STORY_HUB_VALUE), so no hub
        cutscene plays by itself
      - maps that belong to no chapter (the prologue, the Pits, minigames...) keep whatever value
        is current
    Every frame, the current chapter's value is copied back into its slot, so a save made in the
    middle of a chapter is up to date.

    Slots only ever go up. A scene played late (a Pixl join after the chapter has moved on, say)
    still sets its old, lower story value; that's ignored, and the chapter's real value comes back
    at the next map change.

    Writing the real story value (instead of intercepting every read of it) means the game's own
    code, which reads it directly, sees the same value as the scripts do.
*/

namespace mod::story_progress {

#define STORY_CONTEXT_NONE -1  // the map isn't in a chapter or a hub: the value carries over
#define STORY_CONTEXT_HUB 0    // Flipside or Flopside
// 1-8: chapters

#define STORY_CHAPTER_COUNT 8

// The hubs' fixed value: after Peach's revival and the Pure Heart placed in Flipside's pillar, so
// the Lineland and Gloam Valley doors are open and no hub cutscene is waiting to play
#define STORY_HUB_VALUE 65

// The story value when the prologue reaches the Lineland door; below it the mod leaves it alone
#define STORY_PROLOGUE_END 11

struct StoryState
{
    s32 context;                         // which chapter (or hub) the current map belongs to
    s32 slots[STORY_CHAPTER_COUNT + 1];  // copy for the developer tools: [0] = hub value, [1-8] = chapters
};

extern StoryState gStory;

// Starts with no chapter. Call once at startup.
void init();

// Forgets the current chapter (on the title screen), so the next map change doesn't save the
// previous file's value into the newly loaded one.
void resetContext();

// Swaps the story value for a map change to nextMap. Call before any of the new map's scripts run.
void onMapChange(const char * nextMap);

// Copies the current chapter's value into its slot. Call every frame in game.
void sync();

// Which chapter (or hub, or none) a map belongs to, from its name.
s32 contextForMap(const char * map);

// A chapter's story value (its starting value if it hasn't been entered yet).
s32 getChapterValue(s32 chapter);

// Whether a chapter's ending has played.
bool isChapterFinished(s32 chapter);

// Moves a chapter's value forward to `value` (never backwards), as if that chapter's own scene had
// set it. Used for story steps vanilla plays in the hubs, where the mod ignores the scenes' writes.
void advanceChapter(s32 chapter, s32 value);

// How many of a chapter's sections (X-1 to X-4) have been reached: 1-4. Chapters 6 and 8 don't
// have four regular sections, so they only report 1, or 4 once finished.
s32 sectionsReached(s32 chapter);

}
