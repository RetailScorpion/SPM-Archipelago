#pragma once

/*
    The connection status in the pause menu: "CONNECTED" with the Archipelago icon at the bottom left
    of the screen while the client is connected to the Archipelago server, "DISCONNECTED" with the
    grey icon otherwise.

    The client writes into the mailbox twice a second whether it's connected, and the game's frame
    counter as it read it. If that echo falls more than a few seconds behind, the client has closed
    (or Dolphin lost it), which also counts as disconnected.

    The pause menu counts as open while its own window (the first in the pause menu's list) is in
    use; the game frees it when the menu closes.
*/

namespace mod::connection_status {

// Draws the status while the pause menu is open. Call every frame while playing.
void tick();

}
