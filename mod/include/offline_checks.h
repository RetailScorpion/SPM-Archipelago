#pragma once

#include <common.h>

/*
    Offline checks: locations collected before the client has put the seed's items in the game.

    Until the client places the seed's items for the loaded save (it sets the mailbox's seedPlaced;
    the mod clears it whenever the save is left, since loading one reloads the vanilla items), every
    location gives the Archipelago item instead of its vanilla one, with "Connect to Archipelago" text,
    and is logged in the save (a bit per location, AP_GSW_OFFLINE_CHECKS). Once connected, the client
    sends the location as usual and delivers its item if it's this world's: the location gave nothing,
    so it isn't counted as found in the game.

    Which flags are locations: location_flags.h, generated with the APworld's location list (the same
    order, so a location's bit is its Archipelago id minus ID_BASE).

    archipelago_item does the swapping (pickups and chests when they're created; joins, gifts and
    Pure Hearts ask active() before giving) and logs the flag when the game sets it.
*/

namespace mod::offline_checks {

// Whether the seed's items aren't in the game yet: locations give the Archipelago item and are logged.
bool active();

// Whether a save flag (GSWF) is a location's.
bool isLocation(s32 flag);

// Logs a location as collected offline. Returns false if the flag isn't a location's.
bool log(s32 flag);

}
