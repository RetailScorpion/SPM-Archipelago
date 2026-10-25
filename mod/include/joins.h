#pragma once

#include <common.h>

/*
    Pixl join scenes.

    Vanilla only starts a join scene while the chapter's story value is below the point where it
    normally happens (Thoreau: < 25), and gives the Pixl at the end of the section if it was missed
    (a "safety net", party_hoken_NN). With chapters played out of order, the story value can pass
    the join without it happening, so the mod:
      - changes each gate from "story value < N" to "join location not collected" (in place: both
        are two-argument IF instructions),
      - routes the join's add-item call through ap_join_gift, which marks the location collected
        (and, until Archipelago is connected, still gives the Pixl),
      - disables the safety net,
      - removes traps that need the Pixl to escape (Thoreau's room: iron bars and a locked exit),
        since the join location can hold any item.
    storyvalue keeps chapter values from going backwards, since a late join scene still sets the
    story value to its original point.

    Done so far: Thoreau. Next: Carrie, Cudge, Dottie (same kind of gate); Boomer and Thudley (gated
    by a SWITCH on the story value); Fleep (gate inside the gift script); Slim (to investigate).
*/

namespace mod::joins {

void init();

}
