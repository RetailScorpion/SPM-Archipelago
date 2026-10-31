#pragma once

#include <common.h>

/*
    Giving items to Mario.

    Items arrive from two places:
      - the Archipelago server, through the mailbox (the client puts them there)
      - the mod itself, for locations it handles (Pixl joins, story gifts): giveLocal
    Each item is given by making it appear on top of Mario as a normal world item. He collects it at
    once, so the game plays its own "You got X!" scene. Only one item is given at a time, and only
    while the player has control (not during a cutscene, a text box or another item-get).

    A few items aren't normal game items and are handled directly instead: Pure Hearts (a save flag
    that opens a door), Cursya Traps, and characters (which crash the world-item pickup).

    The save file remembers how many server items it has handled (AP_GSW_RECV_COUNT), so loading an
    older save gets the missing ones delivered again.

    This file also hooks the game's "add an item to the inventory" function (pouchAddItem), which
    every collected item goes through.
*/

namespace mod::received_items {

// Hooks the game's inventory function. Call once at startup.
void init();

// Gives an item from one of the mod's own locations: queued, then delivered like a server item.
void giveLocal(s32 itemId);

// Puts the save's handled-item count in the mailbox for the client. Call every frame in game,
// before the game's own frame.
void publishSavedCount();

// Delivers the next waiting item if the player has control. Call every frame in game.
void tick();

// A map change deletes world items: forgets the item on its way, so it's delivered again in the
// new map. Call on every map change.
void onMapChange();

// No save is loaded (title screen, game over, file select): drops every waiting delivery; the
// client queues them again from the next save's count.
void onNoSaveLoaded();

// Whether an item is being delivered or waiting to be.
bool isBusy();

}
