/*
    SPM Archipelago mod: proof of concept.
    - Shows a banner on the title screen, to confirm the mod is loaded
    - Gives items the client queues in the mailbox, through the game's own pickup
      (Mario holds the item up, "You got X!")
    - Reports item pickups to the client as stand-in location checks
*/

#include <common.h>
#include <spm/fontmgr.h>
#include <spm/itemdrv.h>
#include <spm/mario.h>
#include <spm/mario_pouch.h>
#include <spm/seqdef.h>
#include <spm/seqdrv.h>
#include <wii/gx.h>
#include <wii/os.h>

#include "mailbox.h"
#include "patch.h"

namespace mod {

#define MOD_VERSION "0.1-poc"

// Stand-in location ids for the proof of concept: picking up item X reports location POC_LOCATION_BASE + X
#define POC_LOCATION_BASE 0x10000

// Name of the world item spawned on Mario to deliver a received item
#define RECV_ITEM_NAME "ap_recv"

// Item id of the received item currently spawned on Mario, waiting to be collected, or -1
static s32 sSpawnedItem = -1;

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

static void spawnNextReceived()
{
    s32 itemId;
    if (sSpawnedItem != -1 || !mailbox::peekReceived(&itemId))
        return;

    wii::mtx::Vec3 & pos = spm::mario::marioGetPtr()->position;
    spm::itemdrv::itemEntry(RECV_ITEM_NAME, itemId, 0, pos.x, pos.y, pos.z, nullptr, 0);
    sSpawnedItem = itemId;
    wii::os::OSReport("[spmap] spawned received item %d\n", itemId);
}

static bool (*pouchAddItemReal)(s32 itemId);
static bool pouchAddItemHook(s32 itemId)
{
    if (sSpawnedItem != -1 && itemId == sSpawnedItem)
    {
        // Mario collected the item we spawned for the client
        sSpawnedItem = -1;
        mailbox::commitReceived();
        wii::os::OSReport("[spmap] delivered received item %d\n", itemId);
    }
    else
    {
        mailbox::sendLocation(POC_LOCATION_BASE + itemId);
        wii::os::OSReport("[spmap] picked up item %d\n", itemId);
    }
    return pouchAddItemReal(itemId);
}

/*
    Sequence hooks
*/

static spm::seqdef::SeqFunc * seq_titleMainReal;
static void seq_titleMainHook(spm::seqdrv::SeqWork * wp)
{
    mailbox::gMailbox.modState = mailbox::STATE_TITLE;
    seq_titleMainReal(wp);
    drawText("SPM Archipelago " MOD_VERSION, 200.0f, {255, 255, 255, 255});
}

static spm::seqdef::SeqFunc * seq_gameMainReal;
static void seq_gameMainHook(spm::seqdrv::SeqWork * wp)
{
    mailbox::gMailbox.modState = mailbox::STATE_IN_GAME;
    seq_gameMainReal(wp);
    spawnNextReceived();
}

static spm::seqdef::SeqFunc * seq_mapChangeInitReal;
static void seq_mapChangeInitHook(spm::seqdrv::SeqWork * wp)
{
    // A map change deletes world items; an uncollected received item gets spawned again in the new map
    if (sSpawnedItem != -1)
    {
        wii::os::OSReport("[spmap] received item %d not collected before map change, will respawn\n", sSpawnedItem);
        sSpawnedItem = -1;
    }
    seq_mapChangeInitReal(wp);
}

void main()
{
    wii::os::OSReport("[spmap] SPM Archipelago " MOD_VERSION " loaded\n");

    mailbox::init();

    seq_titleMainReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main = seq_titleMainHook;
    seq_gameMainReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_GAME].main;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_GAME].main = seq_gameMainHook;
    seq_mapChangeInitReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_MAPCHANGE].init;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_MAPCHANGE].init = seq_mapChangeInitHook;

    pouchAddItemReal = patch::hookFunction(spm::mario_pouch::pouchAddItem, pouchAddItemHook);
}

}
