#pragma once

#include <common.h>

/*
    Flipside Tower's dimension doors.

    In vanilla the tower's setup script (mac_door_setup_evt) decides everything from the single story
    value: how many doors are open, and which get the section-select menu (every finished chapter;
    the newest door only once its chapter's ending has played). With per-chapter story values, the
    hub's fixed value says nothing about a chapter's progress, so the mod points the tower at its own
    copy of that script.

    The mod's menu lists only the sections a chapter has reached, so leaving mid-chapter (Return
    Pipe from 1-3, say) doesn't mean replaying from 1-1. A door gets the menu once its chapter has
    reached its second section; before that it's a plain door to X-1.

    Which doors are open still comes from the hub value for now; pillars will decide that later.
*/

namespace mod::doors {

/*
    Redirects the tower's setup to the mod's script. Call once at boot (relF is already loaded).
*/
void init();

}
