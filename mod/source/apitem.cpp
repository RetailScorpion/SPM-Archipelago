#include <common.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/item_data.h>
#include <spm/itemdrv.h>
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

// Names for the AP item being collected now (or about to be)
static const LocName * sCurrent = &sPlaceholder;

// EvtVar encoding of GSWF flags (evt_cmd.h)
#define GSWF_VAR_BASE (-130000000)
#define GSWF_COUNT 8192

static const LocName * findLocation(s32 flag)
{
    for (auto & loc : sLocNames)
    {
        if (loc.flag != 0 && loc.flag == flag)
            return &loc;
    }
    return nullptr;
}

// Frames the AP item's item-get stays active after the last frame of its collection. The game looks
// up item_get (and the item name) more than once per item-get, so the text must hold for all of them
#define ACTIVE_FRAMES 30
static s32 sActiveFrames;

static char sGetText[128];

// A location's flag being set: remembers its names for the coming item-get. Chests and Pixl joins set
// their flag before the item appears; pickups set it only after their item-get, so they're handled
// by itemCollectPouchItemHook instead. Only remembers: the message is armed by the AP item itself.
static void (*swSetReal)(s32 id);
static void swSetHook(s32 id)
{
    swSetReal(id);
    if (const LocName * loc = findLocation(id))
        sCurrent = loc;
}

// Runs every frame of an item-get (pickups, chests, deliveries: all spawn an item Mario collects). For
// the AP item it keeps the item-get active, and on the first frame takes the names from the item's own
// flag (pickups; chests and joins have no flag on the item, but set theirs earlier, see swSetHook)
static bool (*itemCollectPouchItemReal)(spm::itemdrv::ItemEntry * item);
static bool itemCollectPouchItemHook(spm::itemdrv::ItemEntry * item)
{
    u16 state = *reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(item) + 0x36);
    if (item->type == AP_ITEM_ID)
    {
        if (state == 0)
        {
            s32 flag = item->switchNumber - GSWF_VAR_BASE;
            if (flag >= 0 && flag < GSWF_COUNT)
            {
                if (const LocName * loc = findLocation(flag))
                    sCurrent = loc;
            }
        }
        sActiveFrames = ACTIVE_FRAMES;
    }
    return itemCollectPouchItemReal(item);
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
    itemCollectPouchItemReal = patch::hookFunction(spm::itemdrv::itemCollectPouchItem, itemCollectPouchItemHook);
    wii::os::OSReport("[spmap] apitem: item %d is the Archipelago item\n", AP_ITEM_ID);
}

void tick()
{
    // The item-get is over: forget the names, so the next AP item doesn't reuse them by mistake
    if (sActiveFrames > 0 && --sActiveFrames == 0)
        sCurrent = &sPlaceholder;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, NAME_KEY) == 0)
        return sCurrent->item;
    if (msl::string::strcmp(msgName, DESC_KEY) == 0)
        return "An item for another world.";
    if (sActiveFrames > 0 && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
    {
        msl::stdio::sprintf(sGetText, "Sent <col c00000ff>%s</col>\nto %s!", sCurrent->item, sCurrent->player);
        return sGetText;
    }
    return nullptr;
}

}
