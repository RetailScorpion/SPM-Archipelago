#pragma once

#include <common.h>

/*
    Flipside Tower's dimension doors.

    In vanilla, the tower's setup script decides everything from the single story value: how many
    doors are open, and which ones get the section-select menu (every finished chapter). With each
    chapter keeping its own story value (story_progress), the hub's fixed value says nothing about a
    chapter's progress, so the mod points the tower at its own copy of that script:
      - door N is open once Pure Heart N is held (pure_hearts)
      - a door gets the section menu once its chapter has reached its second section, and the menu
        lists only the sections that chapter has reached, so leaving in the middle of a chapter
        (with the Return Pipe from 1-3, say) doesn't mean starting again from 1-1
    Chapter 8's door is in Flopside and isn't affected.
*/

namespace mod::tower_doors {

// Points the tower at the mod's door script. Call once at startup (relF is already loaded then).
void init();

// The mod's text for a game message name (the section menus), or nullptr to use the game's own.
const char * message(const char * msgName);

}
