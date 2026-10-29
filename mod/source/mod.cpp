/*
    SPM Archipelago mod: proof of concept.
    - Shows a banner on the title screen, to confirm the mod is loaded
    - Gives items the client queues in the mailbox, through the game's own pickup
      (Mario holds the item up, "You got X!")
    - Reports item pickups to the client as stand-in location checks
*/

#include <common.h>
#include <spm/fontmgr.h>
#include <spm/item_data_ids.h>
#include <spm/itemdrv.h>
#include <spm/mario.h>
#include <spm/mario_pouch.h>
#include <spm/seqdef.h>
#include <spm/seqdrv.h>
#include <spm/spmario.h>
#include <wii/gx.h>
#include <wii/os.h>

#include "apitem.h"
#include "cutscenes.h"
#include "delivery.h"
#include "doors.h"
#include "filesel.h"
#include "flip.h"
#include "gamesyms.h"
#include "hubstory.h"
#include "joins.h"
#include "mailbox.h"
#include "patch.h"
#include "savedata.h"
#include "saveblock.h"
#include "storyvalue.h"
#include "tippi.h"

namespace mod {

// Matches the APworld's world_version (apworld/spm/archipelago.json); shown on the title screen
#define MOD_VERSION "0.1.2"

// Stand-in location ids for the proof of concept: picking up item X reports location POC_LOCATION_BASE + X
#define POC_LOCATION_BASE 0x10000

// Name of the world item spawned on Mario to deliver a received item
#define RECV_ITEM_NAME "ap_recv"

// Item id of the received item currently spawned on Mario, waiting to be collected, or -1
static s32 sSpawnedItem = -1;
// Whether that item came from delivery::giveLocal rather than the client
static bool sSpawnedLocal;

// Items from delivery::giveLocal, waiting to be spawned (oldest first)
#define LOCAL_QUEUE_SIZE 8
static s32 sLocalQueue[LOCAL_QUEUE_SIZE];
static s32 sLocalCount;

static void drawText(const char * msg, f32 y, wii::gx::GXColor colour)
{
    f32 scale = 0.8f;
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&colour);
    spm::fontmgr::FontDrawScale(scale);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    f32 x = -((spm::fontmgr::FontGetMessageWidth(msg) * scale) / 2);
    spm::fontmgr::FontDrawString(x, y, msg);
}

/*
    Received items: spawned as a world item on Mario, so collecting it plays the game's own
    item-get sequence. Only one at a time; it's committed in the mailbox once collected.
*/

// Frames to wait after a delivery before the next one, so the previous item-get sequence has started
#define RECV_COOLDOWN_FRAMES 30

static s32 sRecvCooldown;

// Frames in a row the player has had control (for the debug warp)
static s32 sControlFrames;
#define WARP_CONTROL_FRAMES 60

// The spawned item reached Mario: take it off its queue
// Items from the server the loaded save has handled (delivered or already given in the game)
static u32 savedRecvCount()
{
    return (savedata::getGswByte(AP_GSW_RECV_COUNT) << 8) | savedata::getGswByte(AP_GSW_RECV_COUNT + 1);
}

static void receivedHandled()
{
    u32 count = savedRecvCount() + 1;
    savedata::setGswByte(AP_GSW_RECV_COUNT, static_cast<u8>(count >> 8));
    savedata::setGswByte(AP_GSW_RECV_COUNT + 1, static_cast<u8>(count));
    // The client reads "nothing pending" before the count, so the count must be current first
    mailbox::gMailbox.savedRecvCount = count;
    mailbox::commitReceived();
}

static void itemDelivered()
{
    sSpawnedItem = -1;
    sRecvCooldown = RECV_COOLDOWN_FRAMES;
    if (sSpawnedLocal)
    {
        sLocalCount--;
        for (s32 i = 0; i < sLocalCount; i++)
            sLocalQueue[i] = sLocalQueue[i + 1];
    }
    else
    {
        receivedHandled();
    }
}

static void spawnNextReceived()
{
    if (sRecvCooldown > 0)
    {
        sRecvCooldown--;
        return;
    }

    // Only while the player has control: not during cutscenes, text boxes or another item-get
    if (sSpawnedItem != -1 || spm::mario::marioCtrlOffChk() || spm::mario::marioKeyOffChk())
        return;
    s32 itemId;
    bool local = sLocalCount > 0;
    if (local)
        itemId = sLocalQueue[0];
    else if (!mailbox::peekReceived(&itemId))
        return;
    sSpawnedLocal = local;

    // An item for this world that was found here: the game already gave it, so it only counts
    if (!local && (itemId & RECV_ALREADY_GIVEN))
    {
        receivedHandled();
        return;
    }

    // A trap from the server: the AP item's item-get, as a trap
    if (!local && itemId == apitem::TRAP_ITEM_ID)
    {
        apitem::armTrap();
        wii::mtx::Vec3 & at = spm::mario::marioGetPtr()->position;
        spm::itemdrv::itemEntry(RECV_ITEM_NAME, apitem::AP_ITEM_ID, 0, at.x, at.y, at.z, nullptr, 0);
        sSpawnedItem = apitem::AP_ITEM_ID;
        wii::os::OSReport("[spmap] spawned received trap\n");
        return;
    }

    // Pure Hearts aren't game items: held as a save flag, which opens their door
    if (!local && doors::isHeartItem(itemId))
    {
        doors::giveHeart(itemId - PURE_HEART_ITEM_FIRST + 1);
        receivedHandled();
        return;
    }

    // Characters crash the world-item pickup (they're never world items in vanilla), so they're
    // added directly for now; they'll need their own presentation
    if (itemId >= ITEM_ID_CHAR_START && itemId < ITEM_ID_CHAR_MAX)
    {
        sSpawnedItem = itemId;
        spm::mario_pouch::pouchAddItem(itemId);  // the hook commits it
        return;
    }

    wii::mtx::Vec3 & pos = spm::mario::marioGetPtr()->position;
    spm::itemdrv::itemEntry(RECV_ITEM_NAME, itemId, 0, pos.x, pos.y, pos.z, nullptr, 0);
    sSpawnedItem = itemId;
    wii::os::OSReport("[spmap] spawned received item %d\n", itemId);

    // World pickups (coins, flowers, world mushrooms) take effect without going through
    // pouchAddItem, so the hook never sees them: count them as delivered once spawned on Mario
    if (itemId < ITEM_ID_KEY_START)
        itemDelivered();
}

static bool (*pouchAddItemReal)(s32 itemId);
static bool pouchAddItemHook(s32 itemId)
{
    if (sSpawnedItem != -1 && itemId == sSpawnedItem)
    {
        itemDelivered();
        wii::os::OSReport("[spmap] delivered received item %d\n", itemId);
    }
    else
    {
        mailbox::sendLocation(POC_LOCATION_BASE + itemId);
        wii::os::OSReport("[spmap] picked up item %d\n", itemId);
    }
    // The AP item only exists for its item-get; it never goes in the inventory
    if (itemId == apitem::AP_ITEM_ID)
        return true;
    bool added = pouchAddItemReal(itemId);
    // A first Pixl gets selected right away (pouchAddItem doesn't select it)
    if (itemId >= ITEM_ID_FAIRY_START && itemId < ITEM_ID_FAIRY_MAX)
        joins::repairPixlSelection();
    return added;
}

void delivery::giveLocal(s32 itemId)
{
    if (sLocalCount == LOCAL_QUEUE_SIZE)
    {
        wii::os::OSReport("[spmap] local item queue full, dropped item %d\n", itemId);
        return;
    }
    sLocalQueue[sLocalCount++] = itemId;
    wii::os::OSReport("[spmap] queued local item %d\n", itemId);
}

/*
    Sequence hooks
*/

static spm::seqdef::SeqFunc * seq_titleMainReal;
static void seq_titleMainHook(spm::seqdrv::SeqWork * wp)
{
    mailbox::Mailbox & mb = mailbox::gMailbox;
    mb.modState = mailbox::STATE_TITLE;
    storyvalue::resetContext();
    // No save loaded: drop queued deliveries; the client queues them again from the next save's count
    mb.savedRecvCount = 0xFFFFFFFF;
    mb.recvHandled = mb.recvCount;
    sSpawnedItem = -1;
    sLocalCount = 0;
    seq_titleMainReal(wp);
    drawText("SPM Archipelago " MOD_VERSION, 200.0f, {255, 255, 255, 255});
}

// Debug warp requested by the client (for testing): change map like a door would
static void handleWarpRequest()
{
    mailbox::Mailbox & mb = mailbox::gMailbox;
    if (mb.warpRequest == 0 || spm::seqdrv::seqGetSeq() != spm::seqdrv::SEQ_GAME)
        return;
    // Wait until the player has had control for a while and no received item is on its way:
    // changing map during a cutscene or item-get can crash (control briefly returns mid item-get)
    s32 pending;
    if (spm::mario::marioCtrlOffChk() || spm::mario::marioKeyOffChk())
        sControlFrames = 0;
    else
        sControlFrames++;
    if (sControlFrames < WARP_CONTROL_FRAMES || sSpawnedItem != -1 || sRecvCooldown > 0
        || sLocalCount > 0 || mailbox::peekReceived(&pending))
        return;
    mb.warpRequest = 0;
    mb.warpMap[sizeof(mb.warpMap) - 1] = '\0';
    mb.warpDoor[sizeof(mb.warpDoor) - 1] = '\0';
    wii::os::OSReport("[spmap] debug warp to %s (%s)\n", mb.warpMap, mb.warpDoor);
    spm::seqdrv::seqSetSeq(spm::seqdrv::SEQ_MAPCHANGE, mb.warpMap, mb.warpDoor[0] ? mb.warpDoor : nullptr);
}

static spm::seqdef::SeqFunc * seq_gameMainReal;
static void seq_gameMainHook(spm::seqdrv::SeqWork * wp)
{
    mailbox::gMailbox.modState = mailbox::STATE_IN_GAME;
    handleWarpRequest();
    mailbox::gMailbox.savedRecvCount = savedRecvCount();
    seq_gameMainReal(wp);
    storyvalue::sync();
    joins::tick();
    apitem::tick();
    spawnNextReceived();
}

static spm::seqdef::SeqFunc * seq_mapChangeInitReal;
static void seq_mapChangeInitHook(spm::seqdrv::SeqWork * wp)
{
    cutscenes::onMapChange(wp);

    // A map change deletes world items; an uncollected received item gets spawned again in the new map
    if (sSpawnedItem != -1)
    {
        wii::os::OSReport("[spmap] received item %d not collected before map change, will respawn\n", sSpawnedItem);
        sSpawnedItem = -1;
    }

    // wp->p0 is the map being loaded; none of its scripts have run yet
    storyvalue::onMapChange(wp->p0);
    tippi::onMapChange(storyvalue::gStory.context, spm::spmario::gp->gsw0);
    joins::onMapChange();
    hubstory::onMapChange(wp->p0);
    doors::onMapChange();
    saveblock::onMapChange(wp->p0);
    apitem::onMapChange();

    seq_mapChangeInitReal(wp);
}

void main()
{
    wii::os::OSReport("[spmap] SPM Archipelago " MOD_VERSION " loaded\n");

    mailbox::init();
    // relF is loaded once and stays; section 5 starts 0xAF300 into the file
    mailbox::gMailbox.relfBase = reinterpret_cast<u32>(gamesyms::relF_section5) - 0xAF300;
    storyvalue::init();
    mailbox::gMailbox.storyState = &storyvalue::gStory;
    doors::init();
    flip::init();
    filesel::init();
    saveblock::init();
    joins::init();
    apitem::init();

    seq_titleMainReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main = seq_titleMainHook;
    seq_gameMainReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_GAME].main;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_GAME].main = seq_gameMainHook;
    seq_mapChangeInitReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_MAPCHANGE].init;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_MAPCHANGE].init = seq_mapChangeInitHook;

    pouchAddItemReal = patch::hookFunction(spm::mario_pouch::pouchAddItem, pouchAddItemHook);
}

}
