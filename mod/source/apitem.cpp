#include <common.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/item_data.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "apitem.h"
#include "mailbox.h"
#include "patch.h"

namespace mod::apitem {

// Added to wicon.bin after the game's 402 icons: 402 progression (colour), 403 filler (greyscale)
#define AP_ICON_PROGRESSION 402

#define NAME_KEY "ap_item_name"
#define DESC_KEY "ap_item_desc"
#define ITEM_GET_KEY "item_get"

#define LOC_NAMES_MAX 256
static LocName sLocNames[LOC_NAMES_MAX];

// Shown when the location isn't in the table (no client, or a delivery test)
static const LocName sPlaceholder = {0, 0, "Placeholder Item", "Placeholder Player"};

// Names for the AP item being collected now
static const LocName * sCurrent = &sPlaceholder;

// The AP item is being collected and its item-get message hasn't been shown yet
static bool sGetPending;

static char sGetText[128];

static void (*swSetReal)(s32 id);
static void swSetHook(s32 id)
{
    swSetReal(id);
    for (auto & loc : sLocNames)
    {
        if (loc.flag != 0 && loc.flag == id)
        {
            sCurrent = &loc;
            sGetPending = true;
            break;
        }
    }
}

void init()
{
    spm::item_data::ItemData & item = spm::item_data::itemDataTable[AP_ITEM_ID];
    item.iconId = AP_ICON_PROGRESSION;
    item.nameMsg = NAME_KEY;
    item.descMsg = DESC_KEY;
    mailbox::gMailbox.locNames = sLocNames;
    mailbox::gMailbox.locNamesMax = LOC_NAMES_MAX;
    swSetReal = patch::hookFunction(spm::swdrv::swSet, swSetHook);
    wii::os::OSReport("[spmap] apitem: item %d is the Archipelago item\n", AP_ITEM_ID);
}

void onSpawn(s32 itemId)
{
    if (itemId != AP_ITEM_ID)
        return;
    // A delivery (debug test, or a join holding another world's item, which sets its flag first)
    if (!sGetPending)
        sCurrent = &sPlaceholder;
    sGetPending = true;
}

void onPouchAdd(s32 itemId)
{
    if (itemId == AP_ITEM_ID)
        sGetPending = true;
    else
        sGetPending = false;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, NAME_KEY) == 0)
        return sCurrent->item;
    if (msl::string::strcmp(msgName, DESC_KEY) == 0)
        return "An item for another world.";
    if (sGetPending && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
    {
        sGetPending = false;
        msl::stdio::sprintf(sGetText, "Sent <col c00000ff>%s</col>\nto %s!", sCurrent->item, sCurrent->player);
        sCurrent = &sPlaceholder;
        return sGetText;
    }
    return nullptr;
}

}
