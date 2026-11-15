#pragma once

#include <common.h>

/*
    Flipside Tower's top (mac_02): Mario sometimes falls through the floor up there. The fix (user,
    2026-11-08): from height 1499 up, his depth is held at -250, where the top's floor is, while in 2D (in 3D he
    moves in depth freely).
    Runs for Archipelago and vanilla files alike.
*/

namespace mod::tower_top {

// Holds Mario's depth on the tower top. Call every frame while playing.
void tick();

}
