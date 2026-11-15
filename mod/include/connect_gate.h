#pragma once

#include <common.h>

/*
    A fresh Archipelago file waits for the server (user, 2026-11-09): until the client has put a seed in
    the game for that file once, the file doesn't know its starting character (so it could use Mario's
    flip for checks it shouldn't have yet), its starting doors and so on. So once a new Archipelago file
    is past its opening, the player gets a text box ("Please connect to an Archipelago server.") and no
    control until the seed is placed; from then on the file is marked (AP_GSWF_CONNECTED) and never waits
    again (user, 2026-11-12: only that first time). The room it waited in was set up without the seed
    (Flipside Tower's starting doors shut), so it's then loaded again, with Mario put back where he
    stood. Files from before this (anything received, or a starting character already given) count as
    connected; vanilla files never wait.
*/

namespace mod::connect_gate {

// Holds an Archipelago file until the client places the seed. Call every frame while playing an
// Archipelago file.
void tick();

// Notes whether the map being loaded is set up without the seed. Call on every map change of an
// Archipelago file, before the game's own.
void onMapChange(const char * map);

// Forgets a waiting box (its script ends with the save). Call while no save is loaded.
void onNoSaveLoaded();

// The text box's message (game_messages), or nullptr.
const char * message(const char * msgName);

}
