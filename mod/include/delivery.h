#pragma once

#include <common.h>

namespace mod::delivery {

/*
    Gives an item found at a location in this game (a stand-in until Archipelago decides what each
    location holds). Delivered like a received item: spawned on Mario once he has control, so the
    game's own item-get plays.
*/
void giveLocal(s32 itemId);

}
