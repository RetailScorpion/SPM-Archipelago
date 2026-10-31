#pragma once

#include <common.h>

/*
    Story gift locations: five things the story gives you that the randomizer turns into locations.
      - Peach joining (Flipside, after her revival: see character_joins)
      - Luigi joining (the Underwhere, 7-1: see character_joins)
      - the Helmet (vanilla: Merlon's Goldfish Bowl quest in the hub; here: given on first entering a
        Chapter 4 map, since Outer Space can't be played without it)
      - Bowser joining (3-1, after his fight: see character_joins)
      - Bestovius's flip lesson (1-1: see npc_story_fixes)

    Each is collected once. Like the Pixl joins, each has its own save flag (AP_GSWF_GIFT) that says
    whether it has been collected, so it doesn't depend on story values. Collecting it gives whatever
    the seed placed there (the client writes it into the mailbox's giftItems), or the vanilla item
    without a client.

    Also here: once the Helmet is collected (or held), Chapter 4's story value moves to 135, where
    Outer Space lets Mario swim.
*/

namespace mod::story_gifts {

// The gift locations, in the order of their save flags and of the mailbox's giftItems
enum Gift
{
    GIFT_PEACH,
    GIFT_LUIGI,
    GIFT_HELMET,
    GIFT_BOWSER,
    GIFT_BESTOVIUS,
};

// Collects a gift location if it isn't collected yet: sets its flag and gives what was placed
// there. Returns the item given, or 0 if it was already collected.
s32 collectGift(Gift gift);

// Collects the Helmet on entering Chapter 4, and moves Chapter 4 on once Mario has it.
// Call on every map change, after story_progress::onMapChange.
void onMapChange();

}
