#pragma once

#include <common.h>

/*
    Mario's flip.

    Vanilla blocks it while the story value is between 1 and 15 (before Bestovius in 1-1), in the
    ability check at 0x80147A08. With per-chapter story values that would mean "no flipping in
    Chapter 1 until Bestovius, flipping everywhere else", so the mod replaces the story value read
    there with its own rule, following the `flip` option:
      start_with: always allowed
      vanilla:    allowed once Chapter 1 has passed Bestovius (its slot reaches 16)
      random:     allowed once the Flip item has been received (AP_GSWF_FLIP)
    Until slot data exists, the mode is fixed to start_with.
*/

namespace mod::flip {

enum FlipMode
{
    FLIP_START_WITH,
    FLIP_VANILLA,
    FLIP_RANDOM,
};

void init();

bool isUnlocked();

}
