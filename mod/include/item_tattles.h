#pragma once

#include <common.h>

/*
    Item tattles (user, 2026-11-16, like hovering over a shop item in Ship of Harkinian): Tippi's pointer
    on the Archipelago item lying in a level says what it is and who it's for, and asks the client to
    send it to the server as a hint.

    Tippi's pointer gets an item's text from the game's item tattle function (search_item_msg, the
    search module's kind 5). For the Archipelago item, the hook builds its own text from the location's
    names (archipelago_item::locationOf) and puts the location's flag in the mailbox's hint list. A
    location collected offline (or with no client yet) says to connect to Archipelago.
*/

namespace mod::item_tattles {

// Hooks the item tattle. Call once at startup.
void init();

}
