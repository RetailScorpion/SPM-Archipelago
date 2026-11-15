#include <common.h>
#include <msl/stdio.h>
#include <spm/mario_pouch.h>
#include <wii/os.h>

#include "game_mode.h"
#include "code_patching.h"
#include "mailbox.h"
#include "save_data.h"
#include "wallet.h"

namespace mod::wallet {

#define GAME_COIN_LIMIT 999  // vanilla's limit (pouchSetCoin / pouchAddCoin)

// Each wallet's size, by how many wallets the save has: the starting one (99; user, 2026-11-08: no
// more 0-coin start), then the three (or four) wallets
static const s32 sSizes[] = {99, 200, 500, GAME_COIN_LIMIT, WALLET_INFINITE};
#define SIZE_COUNT (sizeof(sSizes) / sizeof(sSizes[0]))

static u32 mode()
{
    return (mailbox::gMailbox.seedFlags >> SEED_WALLET_SHIFT) & SEED_WALLET_MASK;
}

static u32 walletsHeld()
{
    return save_data::getGswByte(AP_GSW_WALLET);
}

s32 capacity()
{
    u32 m = mode();
    if (m == SEED_WALLET_OFF)
        return GAME_COIN_LIMIT;
    // Without the infinite wallet in the pool, the third wallet (999) is the last
    u32 last = m == SEED_WALLET_INFINITE ? SIZE_COUNT - 1 : SIZE_COUNT - 2;
    u32 held = walletsHeld();
    return sSizes[held < last ? held : last];
}

// Coins cut down to the wallet (and never below 0).
static void trim()
{
    spm::mario_pouch::MarioPouchWork * pouch = spm::mario_pouch::pouchGetPtr();
    s32 cap = capacity();
    if (pouch->coins > cap)
        pouch->coins = cap;
    if (pouch->coins < 0)
        pouch->coins = 0;
}

// ---- Receiving one ----

// How many coins Mario can hold with one more wallet.
static s32 nextCapacity()
{
    u32 m = mode();
    if (m == SEED_WALLET_OFF)
        return GAME_COIN_LIMIT;
    u32 last = m == SEED_WALLET_INFINITE ? SIZE_COUNT - 1 : SIZE_COUNT - 2;
    u32 held = walletsHeld() + 1;
    return sSizes[held < last ? held : last];
}

// The item-get says the new size in the same box (user, 2026-11-13: no second text box). The wallet
// is counted after the item-get, so the size is one wallet up from now.
void appendItemGetLine(char * end)
{
    s32 size = nextCapacity();
    if (size >= WALLET_INFINITE)
        msl::stdio::sprintf(end, "\nYou now have infinite coins");
    else
        msl::stdio::sprintf(end, "\nYou can now hold %d coins", size);
}

void giveWallet()
{
    u32 held = walletsHeld();
    if (held < 255)
        save_data::setGswByte(AP_GSW_WALLET, static_cast<u8>(held + 1));
    wii::os::OSReport("[spmap] wallet: %d held, carries %d coins\n", held + 1, capacity());
}

// ---- The game's coin functions ----

/*
    Both run the game's own function first (pouchAddCoin also adds to the all-time coin count), which caps
    at 999, then set the coins from the amount before that cap, capped at the wallet instead.
*/
static void (*pouchSetCoinReal)(s32 coins);
static void pouchSetCoinHook(s32 coins)
{
    pouchSetCoinReal(coins);
    spm::mario_pouch::pouchGetPtr()->coins = coins;
    trim();
}

static void (*pouchAddCoinReal)(s32 increase);
static void pouchAddCoinHook(s32 increase)
{
    s32 coins = spm::mario_pouch::pouchGetPtr()->coins + increase;
    pouchAddCoinReal(increase);
    spm::mario_pouch::pouchGetPtr()->coins = coins;
    trim();
}

void tick()
{
    trim();
}

void init()
{
    pouchSetCoinReal = game_mode::hook(spm::mario_pouch::pouchSetCoin, pouchSetCoinHook);
    pouchAddCoinReal = game_mode::hook(spm::mario_pouch::pouchAddCoin, pouchAddCoinHook);
}

}
