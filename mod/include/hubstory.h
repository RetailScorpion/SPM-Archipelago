#pragma once

#include <common.h>

/*
    Story steps vanilla plays in the hubs (or tied to story values the per-chapter values skip past),
    given directly instead. The hubs sit at a fixed story value (STORY_HUB_VALUE), so their story
    scenes never play; the mod gives what they give, when the chapter that leads to them is done:
      - Peach joins: when Chapter 1 is finished (vanilla: Flipside Tower scene, story value 63)
      - Luigi joins: on first entering a Chapter 7 map (vanilla: the Underwhere, story value 297, before
        Chapter 7's start value)
      - the Helmet: on first entering a Chapter 4 map (vanilla: Merlon's Goldfish Bowl quest in the hub,
        story values 131-134; without it Outer Space's movement breaks)
      - Sammer's Kingdom is destroyed: Chapter 6 moves from 280 (the duels done) to 281 (vanilla:
        Flipside Tower scene), so the next visit goes to the empty world
      - the Castle Bleck door: once Chapters 1-7 are finished, Flopside Tower (mac_12) gets story value
        357 while Mario is there, which switches its door doa6_l on with Nolrem's door event (story
        values 356-358); going through it plays his last lines and enters Chapter 8

    Peach, Luigi and the Helmet given directly are stand-ins (user's decision, 2026-10-26). Once they're
    shuffled, each becomes a location like the Pixl joins (joins.cpp): its scene is gated on the
    location's own flag instead of the story value, and gives whatever the seed placed there.
*/

namespace mod::hubstory {

/*
    Call on every map change, after storyvalue::onMapChange (it uses the new map's context).
*/
void onMapChange(const char * nextMap);

}
