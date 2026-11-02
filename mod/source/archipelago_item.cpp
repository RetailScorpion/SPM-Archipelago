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

#include "archipelago_item.h"
#include "code_patching.h"
#include "game_addresses.h"
#include "mailbox.h"

namespace mod::archipelago_item {

// Icon numbers the patcher adds after the game's own icons (it skips 403, which the game reserves
// and won't draw)
#define AP_ICON_PROGRESSION 402  // colour
#define AP_ICON_FILLER 404       // grey
#define AP_ICON_BLANK 405        // a trap's item-get: Mario holds up nothing
#define ICON_PURE_HEART_FIRST 0x31  // the game's Pure Heart icons, red (heart 1) to white (heart 8)
#define PURE_HEART_COUNT 8

// Message names the item uses, which game_messages answers with the text below
#define NAME_KEY "ap_item_name"
#define DESC_KEY "ap_item_desc"
#define ITEM_GET_KEY "item_get"  // the game's own "You got X!" message

// How scripts refer to save flags (evt_cmd.h), and how many there are
#define GSWF_VAR_BASE (-130000000)
#define GSWF_COUNT 8192

// The client's list of locations holding other worlds' items
#define LOC_NAMES_MAX 256
static LocName sLocNames[LOC_NAMES_MAX];

// Shown when the location isn't in the list (no client connected, or a test)
static const LocName sPlaceholder = {0, LOC_KIND_OTHER_WORLD, "Placeholder Item", "Placeholder Player"};
// A trap received from the server
static const LocName sReceivedTrap = {0, LOC_KIND_TRAP, "Cursya Trap", ""};

// The names for the Archipelago item being collected now (or about to be)
static const LocName * sCurrent = &sPlaceholder;

// Frames the item-get stays "active" after its last frame. The game looks up the item-get text
// (and the item name) more than once per item-get, so the mod's text must last through all of them
#define ACTIVE_FRAMES 30
static s32 sActiveFrames;

// Whether the current item-get is a trap (its curse comes when it ends)
static bool sTrapActive;

// The "Sent X to Y!" text being shown
static char sItemGetText[128];

// ---- Finding a location's names ----

// The client's entry for the location with this save flag, or nullptr.
static const LocName * findLocation(s32 flag)
{
    for (auto & loc : sLocNames)
    {
        if (loc.flag != 0 && loc.flag == flag)
            return &loc;
    }
    return nullptr;
}

// Turns a script's flag reference into a flag number; returns false if it isn't a save flag.
static bool flagFromScriptValue(s32 value, s32 * flag)
{
    *flag = value - GSWF_VAR_BASE;
    return *flag >= 0 && *flag < GSWF_COUNT;
}

// Whether a location holds another world's filler (grey icon).
static bool isFiller(const LocName * loc)
{
    return loc != nullptr && loc->kind == LOC_KIND_OTHER_WORLD_FILLER;
}

// Which of this world's Pure Hearts a location holds (1-8), or 0 if it's not one.
static s32 pureHeart(const LocName * loc)
{
    if (loc == nullptr || loc->kind <= LOC_KIND_PURE_HEART || loc->kind > LOC_KIND_PURE_HEART + PURE_HEART_COUNT)
        return 0;
    return loc->kind - LOC_KIND_PURE_HEART;
}

// The icon the Archipelago item shows for a location: grey for filler, a Pure Heart's own, or the
// colour one.
static s16 iconFor(const LocName * loc)
{
    if (isFiller(loc))
        return AP_ICON_FILLER;
    if (s32 heart = pureHeart(loc))
        return ICON_PURE_HEART_FIRST + heart - 1;
    return AP_ICON_PROGRESSION;
}

/*
    Chests: a chest sets its flag only after its item-get, and its item carries no flag. So the mod
    remembers every chest the map creates (by hooking the script function that makes them), and an
    item with no flag of its own takes the names of the nearest unopened listed chest.
*/
struct Chest
{
    s32 flag;
    f32 x, y, z;
};
#define CHESTS_MAX 16
static Chest sChests[CHESTS_MAX];
static s32 sChestCount;
// How far from its chest a chest's item can appear
#define CHEST_ITEM_RANGE 200.0f

// Remembers each chest the map creates, then lets the game make it.
// Arguments of evt_mobj_thako: type, name, x, y, z, interactScript, openScript, item, opened flag
static s32 (*evtMobjThakoReal)(spm::evtmgr::EvtEntry * evt, bool isFirstCall);
static s32 evtMobjThakoHook(spm::evtmgr::EvtEntry * evt, bool isFirstCall)
{
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    s32 flag;
    if (isFirstCall && flagFromScriptValue(args[8], &flag) && sChestCount < CHESTS_MAX)
    {
        sChests[sChestCount++] = {flag, spm::evtmgr_cmd::evtGetFloat(evt, args[2]),
                                  spm::evtmgr_cmd::evtGetFloat(evt, args[3]), spm::evtmgr_cmd::evtGetFloat(evt, args[4])};
    }
    return evtMobjThakoReal(evt, isFirstCall);
}

// The names of the nearest unopened chest within range whose location is listed, or nullptr.
static const LocName * nearestChest(const wii::mtx::Vec3 & at)
{
    const LocName * best = nullptr;
    f32 bestDistance = CHEST_ITEM_RANGE * CHEST_ITEM_RANGE;
    for (s32 i = 0; i < sChestCount; i++)
    {
        const Chest & chest = sChests[i];
        const LocName * loc = findLocation(chest.flag);
        if (loc == nullptr || spm::swdrv::swGet(chest.flag))
            continue;
        f32 dx = chest.x - at.x, dy = chest.y - at.y, dz = chest.z - at.z;
        f32 distance = dx * dx + dy * dy + dz * dz;  // squared: no square root needed to compare
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = loc;
        }
    }
    return best;
}

void onMapChange()
{
    sChestCount = 0;
}

// ---- Traps ----

/*
    Cursya curses, weighted by how many of each Cursya the game's maps place: Cursya 8 (slow),
    Reversya 6 (flipped controls), Heavy Cursya 6 (no jump), Tech Cursya 3 (no skills). Back Cursya
    isn't one of the game's statuses, and the Dark ones only appear in the Pits.
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
// The level a Cursya's touch applies
#define CURSE_LEVEL 2

// Gives Mario a random curse, by weight.
static void curse()
{
    s32 total = 0;
    for (auto & c : sCurses)
        total += c.weight;
    s32 roll = spm::system::irand(total - 1);  // 0 to total - 1
    for (auto & c : sCurses)
    {
        if (roll < c.weight)
        {
            game_addresses::mario_status_apply(c.status, CURSE_LEVEL);
            wii::os::OSReport("[spmap] apitem: trap, cursed with %s\n", c.name);
            return;
        }
        roll -= c.weight;
    }
}

void armTrap()
{
    sCurrent = &sReceivedTrap;
}

// ---- The item-get ----

/*
    Runs whenever the game creates a world item. For the Archipelago item at a filler location, the
    item is created with the grey icon (a world item keeps the icon it's created with). Which
    location: the item's own flag (pickups), else the nearest chest (a chest's item), else the
    names already remembered (joins and gifts set their flag first, see swSetHook).
*/
static spm::itemdrv::ItemEntry * (*itemEntryReal)(const char * name, s32 type, s32 behaviour, f32 x, f32 y, f32 z,
                                                  spm::evtmgr::EvtScriptCode * pickupScript, s32 switchNumber);
static spm::itemdrv::ItemEntry * itemEntryHook(const char * name, s32 type, s32 behaviour, f32 x, f32 y, f32 z,
                                              spm::evtmgr::EvtScriptCode * pickupScript, s32 switchNumber)
{
    if (type != AP_ITEM_ID)
        return itemEntryReal(name, type, behaviour, x, y, z, pickupScript, switchNumber);

    s32 flag;
    const wii::mtx::Vec3 at = {x, y, z};
    const LocName * loc = flagFromScriptValue(switchNumber, &flag) ? findLocation(flag) : nearestChest(at);
    if (loc == nullptr)
        loc = sCurrent;

    spm::item_data::ItemData & data = spm::item_data::itemDataTable[AP_ITEM_ID];
    s16 icon = data.iconId;
    data.iconId = iconFor(loc);
    spm::itemdrv::ItemEntry * entry = itemEntryReal(name, type, behaviour, x, y, z, pickupScript, switchNumber);
    data.iconId = icon;
    return entry;
}

// Runs whenever a save flag is set: if it's a listed location's, remembers its names for the coming
// item-get. Chests and Pixl joins set their flag before the item appears; pickups only after their
// item-get, so those are handled in itemCollectPouchItemHook instead.
static void (*swSetReal)(s32 id);
static void swSetHook(s32 id)
{
    swSetReal(id);
    if (const LocName * loc = findLocation(id))
        sCurrent = loc;
}

// The item-get's progress: 0 on its first frame (a field the game keeps at offset 0x36 of the item)
static u16 itemGetStage(spm::itemdrv::ItemEntry * item)
{
    return *reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(item) + 0x36);
}

// On the item-get's first frame: works out the location's names, and sets up a trap or the grey icon.
static void startItemGet(spm::itemdrv::ItemEntry * item)
{
    s32 flag;
    if (flagFromScriptValue(item->switchNumber, &flag))
    {
        if (const LocName * loc = findLocation(flag))
            sCurrent = loc;
    }
    else if (sCurrent == &sPlaceholder)
    {
        if (const LocName * loc = nearestChest(item->position))
            sCurrent = loc;
    }

    if (sCurrent->kind == LOC_KIND_TRAP && !sTrapActive)
    {
        // A trap, whatever it looks like: the Archipelago item with a blank icon
        item->type = AP_ITEM_ID;
        spm::item_data::itemDataTable[AP_ITEM_ID].iconId = AP_ICON_BLANK;
        sTrapActive = true;
    }
    else if (item->type == AP_ITEM_ID)
    {
        // Filler or a Pure Heart: Mario holds up its icon (tick puts the colour one back afterwards)
        spm::item_data::itemDataTable[AP_ITEM_ID].iconId = iconFor(sCurrent);
    }
}

// Runs every frame of every item-get (pickups, chests, deliveries). For the Archipelago item it
// keeps the item-get marked active.
static bool (*itemCollectPouchItemReal)(spm::itemdrv::ItemEntry * item);
static bool itemCollectPouchItemHook(spm::itemdrv::ItemEntry * item)
{
    if (itemGetStage(item) == 0)
        startItemGet(item);
    if (item->type == AP_ITEM_ID)
        sActiveFrames = ACTIVE_FRAMES;
    return itemCollectPouchItemReal(item);
}

void tick()
{
    if (sActiveFrames == 0 || --sActiveFrames != 0)
        return;
    // The item-get is over: forget its names so the next Archipelago item can't reuse them
    sCurrent = &sPlaceholder;
    spm::item_data::itemDataTable[AP_ITEM_ID].iconId = AP_ICON_PROGRESSION;
    if (sTrapActive)
    {
        sTrapActive = false;
        curse();
    }
}

const char * message(const char * msgName)
{
    if (sTrapActive && msl::string::strcmp(msgName, NAME_KEY) == 0)
        return "Nothing";
    if (msl::string::strcmp(msgName, NAME_KEY) == 0)
        return sCurrent->item;
    if (msl::string::strcmp(msgName, DESC_KEY) == 0)
    {
        if (sTrapActive)
            return "That means this is a trap.";
        return pureHeart(sCurrent) ? "One of the eight Pure Hearts." : "An item for another world.";
    }
    if (sTrapActive && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
        return "You are a <col c00000ff>FOOL</col>!";
    if (sActiveFrames > 0 && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0 && pureHeart(sCurrent))
    {
        msl::stdio::sprintf(sItemGetText, "You got <col c00000ff>%s</col>!", sCurrent->item);
        return sItemGetText;
    }
    if (sActiveFrames > 0 && msl::string::strcmp(msgName, ITEM_GET_KEY) == 0)
    {
        msl::stdio::sprintf(sItemGetText, "Sent <col c00000ff>%s</col>\nto %s!", sCurrent->item, sCurrent->player);
        return sItemGetText;
    }
    return nullptr;
}

void init()
{
    spm::item_data::ItemData & item = spm::item_data::itemDataTable[AP_ITEM_ID];
    item.iconId = AP_ICON_PROGRESSION;
    item.nameMsg = NAME_KEY;
    item.descMsg = DESC_KEY;
    mailbox::gMailbox.locNames = sLocNames;
    mailbox::gMailbox.locNamesMax = LOC_NAMES_MAX;
    swSetReal = code_patching::hookFunction(spm::swdrv::swSet, swSetHook);
    itemCollectPouchItemReal = code_patching::hookFunction(spm::itemdrv::itemCollectPouchItem, itemCollectPouchItemHook);
    evtMobjThakoReal = code_patching::hookFunction(spm::evt_mobj::evt_mobj_thako, evtMobjThakoHook);
    itemEntryReal = code_patching::hookFunction(spm::itemdrv::itemEntry, itemEntryHook);
    wii::os::OSReport("[spmap] apitem: item %d is the Archipelago item\n", AP_ITEM_ID);
}

}
