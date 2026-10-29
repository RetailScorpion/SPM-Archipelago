#include <common.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/evt_mobj.h>
#include <spm/evtmgr_cmd.h>
#include <spm/item_data.h>
#include <spm/itemdrv.h>
#include <spm/mario_status.h>
#include <spm/swdrv.h>
#include <spm/system.h>
#include <wii/os.h>

#include "apitem.h"
#include "gamesyms.h"
#include "mailbox.h"
#include "patch.h"

namespace mod::apitem {

// Added to wicon.bin after the game's 402 icons: 402 progression (colour), 403 filler (greyscale)
#define AP_ICON_PROGRESSION 402
#define AP_ICON_BLANK 404  // a trap's item-get: Mario holds up nothing

#define NAME_KEY "ap_item_name"
#define DESC_KEY "ap_item_desc"
#define ITEM_GET_KEY "item_get"

#define LOC_NAMES_MAX 256
static LocName sLocNames[LOC_NAMES_MAX];

// Shown when the location isn't in the table (no client, or a delivery test)
static const LocName sPlaceholder = {0, LOC_KIND_OTHER_WORLD, "Placeholder Item", "Placeholder Player"};
// A trap received from the server
static const LocName sReceivedTrap = {0, LOC_KIND_TRAP, "Cursya Trap", ""};

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

/*
    Chests set their flag only after their item-get, and their item carries no flag. So the chests
    the map creates are remembered (evt_mobj_thako), and an item-get with no flag of its own takes
    the names of the nearest unopened chest whose location is in the table.
*/
struct Chest
{
    s32 flag;
    f32 x, y, z;
};
#define CHESTS_MAX 16
static Chest sChests[CHESTS_MAX];
static s32 sChestCount;
// How far a chest's item can appear from the chest
#define CHEST_ITEM_RANGE 200.0f

static s32 (*evtMobjThakoReal)(spm::evtmgr::EvtEntry * evt, bool isFirstCall);
static s32 evtMobjThakoHook(spm::evtmgr::EvtEntry * evt, bool isFirstCall)
{
    // evt_mobj_thako(type, name, x, y, z, interactScript, openScript, item, opened flag)
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    s32 flag = args[8] - GSWF_VAR_BASE;
    if (isFirstCall && flag >= 0 && flag < GSWF_COUNT && sChestCount < CHESTS_MAX)
    {
        sChests[sChestCount++] = {flag, spm::evtmgr_cmd::evtGetFloat(evt, args[2]),
                                  spm::evtmgr_cmd::evtGetFloat(evt, args[3]), spm::evtmgr_cmd::evtGetFloat(evt, args[4])};
    }
    return evtMobjThakoReal(evt, isFirstCall);
}

static const LocName * nearestChest(const wii::mtx::Vec3 & at)
{
    const LocName * best = nullptr;
    f32 bestDist = CHEST_ITEM_RANGE * CHEST_ITEM_RANGE;
    for (s32 i = 0; i < sChestCount; i++)
    {
        const Chest & c = sChests[i];
        const LocName * loc = findLocation(c.flag);
        if (loc == nullptr || spm::swdrv::swGet(c.flag))
            continue;
        f32 dx = c.x - at.x, dy = c.y - at.y, dz = c.z - at.z;
        f32 dist = dx * dx + dy * dy + dz * dz;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = loc;
        }
    }
    return best;
}

void onMapChange()
{
    sChestCount = 0;
}

// Frames the AP item's item-get stays active after the last frame of its collection. The game looks
// up item_get (and the item name) more than once per item-get, so the text must hold for all of them
#define ACTIVE_FRAMES 30
static s32 sActiveFrames;

static char sGetText[128];

// The current item-get is a trap: its curse comes when it ends
static bool sTrapActive;

/*
    Cursya curses, weighted by how many of each Cursya the game's map setups place: Cursya 8 (slow),
    Reversya 6 (flipped controls), Heavy 6 (no jump), Tech 3 (no skills). Back Cursya (2) isn't one
    of the game's statuses and the Dark ones only appear in the Pits.
*/
static const struct
{
    s32 status;
    s32 weight;
    const char * name;
} sCurses[] = {
    {STATUS_SLOW, 8, "slow"},
    {STATUS_FLIPPED_CONTROLS, 6, "flipped controls"},
    {STATUS_NO_JUMP, 6, "no jump"},
    {STATUS_NO_SKILLS, 3, "no skills"},
};
// The level the Cursyas' touch applies (marioTakeDamage)
#define CURSE_LEVEL 2

static void curse()
{
    s32 total = 0;
    for (auto & c : sCurses)
        total += c.weight;
    s32 roll = spm::system::irand(total - 1);  // 0 to limit, inclusive
    for (auto & c : sCurses)
    {
        if (roll < c.weight)
        {
            gamesyms::mario_status_apply(c.status, CURSE_LEVEL);
            wii::os::OSReport("[spmap] apitem: trap, cursed with %s\n", c.name);
            return;
        }
        roll -= c.weight;
    }
}

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
    if (state == 0)
    {
        s32 flag = item->switchNumber - GSWF_VAR_BASE;
        if (flag >= 0 && flag < GSWF_COUNT)
        {
            if (const LocName * loc = findLocation(flag))
                sCurrent = loc;
        }
        else if (sCurrent == &sPlaceholder)
        {
            if (const LocName * loc = nearestChest(item->position))
                sCurrent = loc;
        }
        // A trap, whatever it looks like: the AP item with a blank icon, before the item-get starts
        if (sCurrent->kind == LOC_KIND_TRAP && !sTrapActive)
        {
            item->type = AP_ITEM_ID;
            spm::item_data::itemDataTable[AP_ITEM_ID].iconId = AP_ICON_BLANK;
            sTrapActive = true;
        }
    }
    if (item->type == AP_ITEM_ID)
        sActiveFrames = ACTIVE_FRAMES;
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
    evtMobjThakoReal = patch::hookFunction(spm::evt_mobj::evt_mobj_thako, evtMobjThakoHook);
    wii::os::OSReport("[spmap] apitem: item %d is the Archipelago item\n", AP_ITEM_ID);
}

void tick()
{
    // The item-get is over: forget the names, so the next AP item doesn't reuse them by mistake
    if (sActiveFrames > 0 && --sActiveFrames == 0)
    {
        sCurrent = &sPlaceholder;
        if (sTrapActive)
        {
            sTrapActive = false;
            spm::item_data::itemDataTable[AP_ITEM_ID].iconId = AP_ICON_PROGRESSION;
            curse();
        }
    }
}

void armTrap()
{
    sCurrent = &sReceivedTrap;
}

const char * message(const char * msgName)
{
    if (sTrapActive && msl::string::strcmp(msgName, NAME_KEY) == 0)
        return "Nothing";
    if (msl::string::strcmp(msgName, NAME_KEY) == 0)
        return sCurrent->item;
    if (msl::string::strcmp(msgName, DESC_KEY) == 0)
        return sTrapActive ? "That means this is a trap." : "An item for another world.";
    if (sTrapActive && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
        return "You are a <col c00000ff>FOOL</col>!";
    if (sActiveFrames > 0 && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
    {
        msl::stdio::sprintf(sGetText, "Sent <col c00000ff>%s</col>\nto %s!", sCurrent->item, sCurrent->player);
        return sGetText;
    }
    return nullptr;
}

}
