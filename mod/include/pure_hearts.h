#pragma once

#include <common.h>

/*
    Pure Hearts.

    In the randomizer, Pure Heart N opens Flipside Tower's door N (Chapter N). The hearts held are
    save flags (AP_GSWF_HEART), not inventory items:
      - the seed's starting hearts come from the client (mailbox startingHearts) and are added to
        the save on every map change
      - the other hearts are items from the server, which the mod turns into the flag when received
    Without a client, every door is open.
*/

namespace mod::pure_hearts {

#define PURE_HEART_COUNT 8
// The ids the server sends for Pure Hearts 1-8 (not game items: Archipelago item number minus the
// world's base)
#define PURE_HEART_ITEM_FIRST 1001

// Whether an item from the server is a Pure Heart.
bool isHeartItem(s32 itemId);

// Gives Pure Heart `heart` (1-8).
void giveHeart(s32 heart);

// Whether the door for Pure Heart `heart` (1-8) is open.
bool isDoorOpen(s32 heart);

// Adds the seed's starting hearts to the save. Call on every map change, before the map's scripts.
void onMapChange();

}
