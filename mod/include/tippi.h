#pragma once

#include <common.h>

/*
    Keeps Tippi (the "guide") with Mario.

    The game saves Tippi's presence as GSWF 403, from bit 0x80 of the guide's flag0 (0x80 = disabled),
    and restores it when a save is loaded. Hub cutscenes swap her for an NPC stand-in, and the real
    guide only switches on late in the prologue, so a save made at the wrong moment (or a story
    value that skips the scene that switches her on) leaves her off for good.

    On every map change the mod switches her on (except in the prologue), including the stretches
    where vanilla takes her away (Chapter 3 with Francis, the Underwhere). Scripts that hide her for a
    cutscene run after this and still can.
*/

namespace mod::tippi {

/*
    Call on every map change, after storyvalue::onMapChange (it uses the new map's story value).
*/
void onMapChange(s32 context, s32 storyValue);

}
