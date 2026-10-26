#include <common.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/item_data.h>
#include <wii/os.h>

#include "apitem.h"

namespace mod::apitem {

// Added to wicon.bin after the game's 402 icons: 402 progression (colour), 403 filler (greyscale)
#define AP_ICON_PROGRESSION 402

#define NAME_KEY "ap_item_name"
#define DESC_KEY "ap_item_desc"
#define ITEM_GET_KEY "item_get"

// Placeholders until the client sends the real item and player names
static const char * sItemName = "Placeholder Item";
static const char * sPlayerName = "Placeholder Player";

// The AP item has been spawned and its item-get message hasn't been shown yet
static bool sGetPending;

static char sGetText[128];

void init()
{
    spm::item_data::ItemData & item = spm::item_data::itemDataTable[AP_ITEM_ID];
    item.iconId = AP_ICON_PROGRESSION;
    item.nameMsg = NAME_KEY;
    item.descMsg = DESC_KEY;
    wii::os::OSReport("[spmap] apitem: item %d is the Archipelago item\n", AP_ITEM_ID);
}

void onSpawn(s32 itemId)
{
    sGetPending = itemId == AP_ITEM_ID;
}

void onPouchAdd(s32 itemId)
{
    if (itemId != AP_ITEM_ID)
        sGetPending = false;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, NAME_KEY) == 0)
        return sItemName;
    if (msl::string::strcmp(msgName, DESC_KEY) == 0)
        return "An item for another world.";
    if (sGetPending && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
    {
        sGetPending = false;
        msl::stdio::sprintf(sGetText, "Sent <col c00000ff>%s</col>\nto %s!", sItemName, sPlayerName);
        return sGetText;
    }
    return nullptr;
}

}
