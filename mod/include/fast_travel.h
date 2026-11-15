#pragma once

#include <common.h>

/*
    Fast travel from the pause menu's Chapters tab (user, 2026-11-13). In a chapter's view (its four
    sections, the hand on one), 2 on a section the tower's door would offer (pause_chapters) asks "Would
    you like to fast travel to X-X?" over the open menu, as the Return Pipe does; No leaves the menu as
    it was, Yes closes it and takes Mario to that section the way
    the tower's door does: the first map and entrance from the door's section table (relF
    evt_mac_door_section_dest), Chapter 6 where its door currently leads (AP_GSW_DOOR_6), Chapter 8 to
    ls<N>_01. Archipelago files only (the hooks go through game_mode).
*/

namespace mod::fast_travel {

// Hooks the chapter view's button reads. Call once at startup.
void init();

// Changes map once the pause menu has closed after a Yes. Call every frame while playing an Archipelago file.
void tick();

// The question and its Yes / No list (game_messages), or nullptr.
const char * message(const char * msgName);

}
