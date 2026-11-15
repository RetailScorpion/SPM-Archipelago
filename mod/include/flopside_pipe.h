#pragma once

#include <common.h>

/*
    The blue pipe between Flipside Tower (mac_02 aodokan_2) and Flopside Tower (mac_12 aodokan_1).

    Both towers hide the pipe and switch it off while save flag GSWF 534 is clear (their setup scripts,
    relF 0xe94a0 / 0xe95d8; 533 and 535 are the towers' pipes to their Pit floors). The mod sets the
    flag once Flopside is in logic: Boomer, Fleep and Mario held (rules.py FLOPSIDE_NEEDS; user,
    2026-11-13; it was Fleep alone from 2026-11-02, which let Bowser reach Flopside). It's set both
    ways, so a save where an older build opened it early loses it until then.
*/

namespace mod::flopside_pipe {

// Opens or closes the pipe. Call on every map change (Archipelago files).
void onMapChange();

}
