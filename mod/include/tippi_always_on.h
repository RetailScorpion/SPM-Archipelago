#pragma once

#include <common.h>

/*
    Keeps Tippi with Mario.

    The game saves whether Tippi is with Mario as save flag 403, and restores her from it when a
    save is loaded. Hub cutscenes swap her for a look-alike character, and the real Tippi only
    switches on late in the prologue, so a save made at the wrong moment (or a story value that
    skips the scene that switches her on) could leave her off for good.

    On every map change the mod switches her on (except during the prologue), including the
    stretches where vanilla takes her away (Chapter 3 with Francis, the Underwhere), since her
    pointer is needed everywhere in a randomizer. Scripts that hide her for a cutscene run after
    this and still can.
*/

namespace mod::tippi_always_on {

// Call on every map change, after story_progress::onMapChange (it uses the new map's story value).
void onMapChange(s32 context, s32 storyValue);

}
