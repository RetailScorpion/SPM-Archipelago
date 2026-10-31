#pragma once

#include <common.h>

/*
    When Mario can flip between 2D and 3D.

    Vanilla blocks flipping while the story value is between 1 and 15 (before Bestovius teaches it
    in 1-1). With each chapter keeping its own story value, that would mean "no flipping in Chapter 1
    until Bestovius, but flipping everywhere else". So the mod replaces the story value read in the
    "can Mario use his ability" check with its own rule, following the flip option:
      start_with: always allowed
      vanilla:    allowed once Chapter 1 has passed Bestovius (its value reaches 16)
      random:     allowed once the Flip item has been received (AP_GSWF_FLIP)
    The option isn't sent by the client yet, so the rule is fixed to start_with for now.
*/

namespace mod::flip_ability {

enum FlipMode
{
    FLIP_START_WITH,
    FLIP_VANILLA,
    FLIP_RANDOM,
};

// Replaces the game's story value check. Call once at startup.
void init();

// Whether Mario can flip right now.
bool isUnlocked();

}
