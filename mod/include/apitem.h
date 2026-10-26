#pragma once

#include <common.h>

/*
    The stand-in item for items that belong to other worlds. Collecting it plays the game's own
    item-get, with Mario holding up the Archipelago icon and "Sent <item> to <player>!" instead of
    "You got <item>!". It's never added to the inventory.

    It reuses item 63 (Cooking Disk 1, never obtainable in vanilla): its item-table entry is pointed
    at the Archipelago icon (added to wicon.tpl/wicon.bin by tools/wicon.py) and the mod's own
    messages. The cooking_disks option will need a different entry.

    The client fills a table of names for the locations holding other worlds' items, keyed by the
    location's save flag. The game sets that flag when the location is collected (swSet), which is
    how the mod knows whose names to show.
*/

namespace mod::apitem {

constexpr s32 AP_ITEM_ID = 63;

// One location holding another world's item (written by the client; mailbox locNames)
struct LocName
{
    u16 flag;        // the location's GSWF save flag; 0 = unused entry
    u16 pad;
    char item[36];   // NUL-terminated
    char player[24];
};
static_assert(sizeof(LocName) == 0x40);

void init();

// Called when the mod spawns an item on Mario for delivery
void onSpawn(s32 itemId);

// Called when an item is added to the inventory (or would be): any other item ends the AP item-get
void onPouchAdd(s32 itemId);

// The mod's text for a message, or nullptr to use the game's
const char * message(const char * msgName);

}
