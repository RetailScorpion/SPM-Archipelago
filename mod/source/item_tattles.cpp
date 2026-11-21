#include <common.h>
#include <msl/stdio.h>
#include <spm/itemdrv.h>
#include <wii/os.h>

#include "archipelago_item.h"
#include "game_addresses.h"
#include "game_mode.h"
#include "item_tattles.h"
#include "mailbox.h"

namespace mod::item_tattles {

// Tippi's text for the Archipelago item. Same layout as her own item tattles (msg anna_item): the
// pointer's tag, her voice, then the text
static char sText[256];

// Locations already sent as hints since the game started (the server would ignore repeats anyway)
#define HINTED_MAX 128
static u16 sHinted[HINTED_MAX];
static s32 sHintedCount;

static void requestHint(u16 flag)
{
    for (s32 i = 0; i < sHintedCount; i++)
    {
        if (sHinted[i] == flag)
            return;
    }
    // A full list (no client reading it) tries again on the next tattle
    if (!mailbox::requestHint(flag))
        return;
    if (sHintedCount < HINTED_MAX)
        sHinted[sHintedCount++] = flag;
    wii::os::OSReport("[spmap] itemtattle: hint asked for location flag %d\n", flag);
}

static const char * (*searchItemMsgReal)(spm::itemdrv::ItemEntry * item);
static const char * searchItemMsgHook(spm::itemdrv::ItemEntry * item)
{
    if (item->type != archipelago_item::AP_ITEM_ID)
        return searchItemMsgReal(item);
    const archipelago_item::LocName * loc = archipelago_item::locationOf(item);
    // A trap looks like a key item, never the Archipelago item; just in case, don't give it away
    if (loc != nullptr && loc->kind == archipelago_item::LOC_KIND_TRAP)
        return searchItemMsgReal(item);
    if (loc == nullptr || loc->flag == 0)
    {
        msl::stdio::sprintf(sText, "<gsearch>\n<fairy><keyyon>\nThat's an Archipelago item.\n<wait 150>"
                                   "Connect to Archipelago to\nfind out what it is...\n<k>");
        return sText;
    }
    msl::stdio::sprintf(sText, "<gsearch>\n<fairy><keyyon>\nThat's an Archipelago item.\n<wait 150>"
                               "It's <col c00000ff>%s</col> \nfor %s!\n<k>",
                        loc->item, loc->player[0] != '\0' ? loc->player : "you");
    requestHint(loc->flag);
    return sText;
}

void init()
{
    searchItemMsgReal = game_mode::hook(game_addresses::search_item_msg, searchItemMsgHook);
}

}
