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

    Each of Chapters 1-7 ends by handing over the next Pure Heart (vanilla: Chapter N gives heart
    N + 1, Chapter 7 heart 8). That's a location (AP_GSWF_CHAPTER_HEART), collected on the first map
    change once the chapter is finished, like the story gifts: the heart's own scene still plays, then
    the item the seed placed there arrives (mailbox heartLocItems), or without a client the vanilla
    heart.
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

// Adds the seed's starting hearts to the save, and collects the Pure Heart locations of finished
// chapters. Call on every map change, after story_progress::onMapChange.
void onMapChange();

}
