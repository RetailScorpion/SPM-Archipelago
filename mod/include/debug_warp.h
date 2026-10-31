#pragma once

#include <common.h>

/*
    Developer feature: warping to any map.

    A developer tool on the PC writes a map name (and optionally a door) into the mailbox and sets
    its warpRequest. The mod then changes map the way going through a door would. It waits until
    the player has had control for a second and no item is being delivered, since changing map in
    the middle of a cutscene or an item-get can crash the game.
*/

namespace mod::debug_warp {

// Warps if a warp was requested and it's safe. Call every frame in game.
void tick();

}
