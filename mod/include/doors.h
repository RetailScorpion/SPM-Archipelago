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

    Door N (Chapters 1-7) is open once Pure Heart N is held (AP_GSWF_HEART): the seed's starting
    hearts (mailbox startingHearts, set into the save on map changes) or one received from the
    server. Pillars aren't involved yet. Without a client every door is open.
*/

namespace mod::doors {

#define PURE_HEART_COUNT 8
// Item ids for Pure Hearts 1-8 from the server (not game items; Archipelago item id - id base)
#define PURE_HEART_ITEM_FIRST 1001

bool isHeartItem(s32 itemId);

// heart: 1-8
void giveHeart(s32 heart);
bool isDoorOpen(s32 heart);

/*
    Gives the save the seed's starting hearts. Call on every map change, before the map's scripts.
*/
void onMapChange();

/*
    Redirects the tower's setup to the mod's script. Call once at boot (relF is already loaded).
*/
void init();

}
