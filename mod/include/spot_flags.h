#pragma once

/*
    Save flags for item spots that have none of their own.

    A location is found by the save flag its item sets when collected, and the client places an item
    there by rewriting the item number in the script command that spawns it. Some spawns pass no
    flag (0), so nothing would mark them collected. The mod gives each of those spawns one of its own
    flags (AP_GSWF_LOC, in sSpots' order); tools/gen_ap_data.py lists the same spots in the same
    order (UNFLAGGED_SPOTS).

    Spots so far: King Sammer's seven TTYD partner Catch Cards (wa4_26, after the 100th duel of
    post-game Sammer's Kingdom). He hands them over one after another, each spawned on Mario and
    collected before the next. His scene only plays once (it ends by setting GSWF 1102), so a
    flagged card that has already been collected can't block it.
*/

namespace mod::spot_flags {

// Gives the spots their flags. Call once at startup.
void init();

}
