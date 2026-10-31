#pragma once

#include <common.h>

/*
    Story steps vanilla plays in the hubs, done by the mod instead.

    The hubs (Flipside and Flopside) always have a fixed story value (story_progress), so their own
    story scenes never play. Two of those scenes move a chapter on, so the mod does that itself:
      - Sammer's Kingdom ends: once the duels are done (Chapter 6 at 280), Chapter 6 moves to 281
        (vanilla does it in a Flipside Tower scene), so the next visit goes to the destroyed world
      - the Castle Bleck door: once Chapters 1-7 are finished, Flopside Tower (mac_12) gets story
        value 357 while Mario is there. That turns on its door the way vanilla's scene does (story
        values 356-358); going through it plays Nolrem's last lines and enters Chapter 8
*/

namespace mod::hub_story_fixes {

// Call on every map change, after story_progress::onMapChange.
void onMapChange(const char * nextMap);

}
