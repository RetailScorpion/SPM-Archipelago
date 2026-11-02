#pragma once

#include <common.h>

/*
    Story steps vanilla plays in the hubs, done by the mod instead.

    The hubs (Flipside and Flopside) always have a fixed story value (story_progress), so their own
    story scenes never play. Two of those scenes move a chapter on, so the mod does that itself:
      - Sammer's Kingdom ends: once the duels are done (Chapter 6 at 280), Chapter 6 moves to 281
        (vanilla does it in a Flipside Tower scene)
      - Chapter 6's door: vanilla picks its destination from the hub's story value, so it's
        patched to follow Chapter 6's own value: Sammer's Kingdom until the duels are done, then
        the World of Nothing, then (once Brobot L-type is beaten, which finishes the chapter)
        post-game Sammer's Kingdom with its 100 duels. For that, Chapter 6's value jumps to
        vanilla's post-game value (422) once Mario has left the finished chapter
      - the blue pipe between Flipside Tower and Flopside Tower (GSWF 534): on once Mario has Fleep
      - the Castle Bleck door: once Chapters 1-7 are finished, Flopside Tower (mac_12) gets story
        value 357 while Mario is there. That turns on its door the way vanilla's scene does (story
        values 356-358); going through it plays Nolrem's last lines and enters Chapter 8

    The hub value is low (the end of the prologue), so a few hub tests are patched to come out as
    they would later in the story: the elevators, a Flopside house door, the Old Key gate and the
    recipe terminals work, and two Peach chain tests don't catch the low value (init).
*/

namespace mod::hub_story_fixes {

// Patches the hub's story value tests. Call once at startup.
void init();

// Call on every map change, after story_progress::onMapChange.
void onMapChange(const char * nextMap);

}
