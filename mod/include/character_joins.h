#pragma once

#include <common.h>

/*
    Peach, Bowser and Luigi joining, as locations (story_gifts).

    Each character's join scene collects that character's gift location instead of adding the
    character. The scene's own "add the character" call is switched off, and its "switch to the
    character" call only switches if Mario actually has them (switching to a character he doesn't
    have breaks his controls).
      - Bowser: 3-1 (ta1_04), after his fight. The fight still waits for Chapter 3's own story value.
      - Luigi: the Underwhere (an1_07). Vanilla plays his scene at story values 295-296, just before
        Chapter 7 starts; now it plays until his location is collected.
      - Peach: her revival chain (below).

    Peach's revival chain has its own story value (AP_GSW_PEACH_CHAIN, 60-63), read and set by the
    patched scripts instead of the real story value, so it's there from the start of the file:
      - 60: Flipside (mac_01) shows her unconscious with Merlon, and plays the "she fell from the
        sky" scene, which moves the chain to 61
      - 61: Saffron cooks a Fire Burst into a Spicy Soup for her (her script in the main program is
        patched too), which moves it to 62
      - talking to Peach while holding a Spicy Soup collects her location and ends the chain (63)
      - with cutscenes on and Peach in the party, the vanilla scenes play instead: the soup takes
        Mario to Merlon's house, where the join scene collects the location
*/

namespace mod::character_joins {

// Patches the join scenes and Saffron's script. Call once at startup.
void init();

// Keeps Peach's chain value valid and sets whether her join scene is due.
// Call on every map change, after story_progress::onMapChange.
void onMapChange();

}
