#pragma once

#include <common.h>

/*
    The pause menu's Chapters tab follows the tower's doors (user, 2026-11-09), not the story value:
    a chapter is listed once its door is open (its Pure Heart; Chapter 8: the castle requirement, plus
    Mario, Boomer and Fleep to reach that door in Flopside), and
    a section once that door's section menu would offer it (the first, then each one the chapter has
    reached; Sammer's Kingdom only its start). The tab compares the story value with a table of each
    section's value (main.dol 0x803EA208, read by pluswinChapterWinOpen and its neighbours in about 25
    places); the mod keeps that table at "always" or "never" for each section instead, through
    game_mode, so vanilla files keep the game's own.
*/

namespace mod::pause_chapters {

// Brings the table up to date with the doors. Call every frame while playing an Archipelago file.
void tick();

// Lets the cursor rest on locked chapters without opening them or showing their name and description.
// Call once at startup.
void init();

// Whether the tab lists a section (chapter 1-8, section 1-4): the tower's door would offer it.
bool sectionReachable(s32 chapter, s32 section);

// The blank text for a locked chapter (game_messages), or nullptr.
const char * message(const char * msgName);

}
