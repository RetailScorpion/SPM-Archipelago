#pragma once

#include <common.h>

/*
    Progressive wallets (progressive_wallet option, user 2026-11-08, as in the Ocarina of Time
    randomizer): the file starts holding up to 99 coins (it started with none at first; the user dropped
    that), and each Progressive Wallet raises it: 200, 500, 999 (the game's own limit), then with
    "with_infinite" a fourth one that lifts the limit (WALLET_INFINITE coins).

    The client puts the option in the mailbox's seedFlags (SEED_WALLET bits; 0 is off: the game's 999).
    Wallets are server items (item WALLET_ITEM_ID), counted in the save (AP_GSW_WALLET). The game caps
    coins in pouchSetCoin and pouchAddCoin only (999, 0x8014CA54 / 0x8014CA98); both are hooked to cap
    at the wallet's size instead, and every frame trims coins above it (a save loaded, or the client
    connecting after coins were picked up without it).
*/

// The Progressive Wallet's item number (not a game item; the APworld's items.py WALLET_GAME_ID)
#define WALLET_ITEM_ID 1200

// The "no limit" wallet's size: more than the counter ever needs, well inside the game's s32
#define WALLET_INFINITE 99999

namespace mod::wallet {

// How many coins Mario can hold now.
s32 capacity();

// Writes the wallet item-get's second line at `end`: "You can now hold N coins" (the size one more
// wallet gives), or "You now have infinite coins".
void appendItemGetLine(char * end);

// A Progressive Wallet arrived: one size up.
void giveWallet();

// Trims coins above the wallet's size. Call every frame.
void tick();

// Hooks the game's coin functions. Call once at startup.
void init();

}
