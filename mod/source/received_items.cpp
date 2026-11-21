#include <common.h>
#include <msl/stdio.h>
#include <spm/item_data_ids.h>
#include <spm/itemdrv.h>
#include <spm/mario.h>
#include <spm/mario_pouch.h>
#include <wii/os.h>

#include "game_mode.h"
#include "archipelago_item.h"
#include "code_patching.h"
#include "key_groups.h"
#include "key_item_storage.h"
#include "mailbox.h"
#include "pixl_joins.h"
#include "pure_hearts.h"
#include "starting_character.h"
#include "received_items.h"
#include "save_data.h"
#include "wallet.h"

namespace mod::received_items {

// The names given to the world items that appear on Mario: "ap_r000" to "ap_r999", a new one each
// time. With one fixed name, an item thrown away from the full-bag menu stays in the room under that
// name, and the next delivery with the same name hangs the game (user, 2026-11-05)
#define DELIVERY_ITEM_NAME_FORMAT "ap_r%03d"
#define DELIVERY_NAME_COUNT 1000
static s32 sDeliveryNameIndex;

// Frames to wait after a delivery before the next one, so the previous item-get has started
#define DELIVERY_COOLDOWN_FRAMES 30

// Developer feature from the first prototype: picking up item X reports location 0x10000 + X to the
// mailbox (only the developer tools client/watch.py and client/poc_client.py read it)
#define PICKUP_LOG_LOCATION_BASE 0x10000

#define NO_ITEM -1

// The item that appeared on Mario and hasn't been collected yet, or NO_ITEM
static s32 sDeliveringItem = NO_ITEM;
// Whether that item came from giveLocal (true) or the server (false)
static bool sDeliveringLocal;
// Whether that item is a Progressive Wallet (spawned as the Archipelago item with the wallet's picture)
static bool sDeliveringWallet;
// The Pure Heart that item is (1-8; spawned as the Archipelago item with the heart's icon), or 0
static s32 sDeliveringHeart;
// Frames left before the next delivery
static s32 sCooldown;

// Items from giveLocal, oldest first
#define LOCAL_QUEUE_SIZE 8
static s32 sLocalQueue[LOCAL_QUEUE_SIZE];
static s32 sLocalCount;

// ---- The save's count of handled server items ----

// How many server items the loaded save has handled (given, or already found in this game).
static u32 savedHandledCount()
{
    return save_data::getGswU16(AP_GSW_RECV_COUNT);
}

// One more server item handled: counts it in the save and takes it off the mailbox list.
static void serverItemHandled()
{
    u32 count = savedHandledCount() + 1;
    save_data::setGswU16(AP_GSW_RECV_COUNT, static_cast<u16>(count));
    // The client checks "nothing waiting" before it reads the count, so the count goes first
    mailbox::gMailbox.savedRecvCount = count;
    mailbox::commitReceived();
}

void publishSavedCount()
{
    mailbox::gMailbox.savedRecvCount = savedHandledCount();
}

// ---- Delivering ----

// Removes the oldest item from the local queue.
static void dropOldestLocal()
{
    sLocalCount--;
    for (s32 i = 0; i < sLocalCount; i++)
        sLocalQueue[i] = sLocalQueue[i + 1];
}

// The delivered item reached Mario: takes it off its list and starts the cooldown.
static void deliveryDone()
{
    sDeliveringItem = NO_ITEM;
    sCooldown = DELIVERY_COOLDOWN_FRAMES;
    if (sDeliveringWallet)
        wallet::giveWallet();
    sDeliveringWallet = false;
    if (sDeliveringHeart != 0)
        pure_hearts::giveHeart(sDeliveringHeart);
    sDeliveringHeart = 0;
    if (sDeliveringLocal)
        dropOldestLocal();
    else
        serverItemHandled();
}

// Makes an item appear on Mario as a world item, so he collects it with the game's item-get.
static void spawnOnMario(s32 itemId)
{
    wii::mtx::Vec3 & at = spm::mario::marioGetPtr()->position;
    char name[16];
    msl::stdio::sprintf(name, DELIVERY_ITEM_NAME_FORMAT, sDeliveryNameIndex);
    sDeliveryNameIndex = (sDeliveryNameIndex + 1) % DELIVERY_NAME_COUNT;
    spm::itemdrv::itemEntry(name, itemId, 0, at.x, at.y, at.z, nullptr, 0);
    sDeliveringItem = itemId;
}

// Whether the player is in control of Mario (no cutscene, text box or item-get going on).
static bool playerHasControl()
{
    return !spm::mario::marioCtrlOffChk() && !spm::mario::marioKeyOffChk();
}

// Gets the next item to deliver: local items first, then the server's. Returns false if none.
static bool nextItem(s32 * itemId, bool * local)
{
    *local = sLocalCount > 0;
    if (*local)
    {
        *itemId = sLocalQueue[0];
        return true;
    }
    return mailbox::peekReceived(itemId);
}

void tick()
{
    if (sCooldown > 0)
    {
        sCooldown--;
        return;
    }
    if (sDeliveringItem != NO_ITEM || !playerHasControl())
        return;

    s32 itemId;
    bool local;
    if (!nextItem(&itemId, &local))
        return;
    sDeliveringLocal = local;

    // One of this world's items found here: the game already gave it, so it's only counted. A wallet
    // found here was the Archipelago item's item-get, so its size goes up now (no second pickup)
    if (!local && (itemId & RECV_ALREADY_GIVEN))
    {
        s32 found = itemId & ~RECV_ALREADY_GIVEN;
        if (found == WALLET_ITEM_ID)
            wallet::giveWallet();
        // A Pure Heart found here was the Archipelago item's item-get ("You got Pure Heart N!"): its
        // door opens now
        if (pure_hearts::isHeartItem(found))
            pure_hearts::giveHeart(found - PURE_HEART_ITEM_FIRST + 1);
        serverItemHandled();
        return;
    }

    // A Cursya Trap from the server: plays as the Archipelago item's item-get, as a trap
    if (!local && itemId == archipelago_item::TRAP_ITEM_ID)
    {
        archipelago_item::armTrap();
        spawnOnMario(archipelago_item::AP_ITEM_ID);
        wii::os::OSReport("[spmap] spawned received trap\n");
        return;
    }

    // A Pure Heart isn't a game item: it's a save flag, which opens its door. It plays as the
    // Archipelago item's item-get with the heart's icon, and the door opens once it's picked up
    if (!local && pure_hearts::isHeartItem(itemId))
    {
        sDeliveringHeart = itemId - PURE_HEART_ITEM_FIRST + 1;
        archipelago_item::armHeart(sDeliveringHeart);
        spawnOnMario(archipelago_item::AP_ITEM_ID);
        wii::os::OSReport("[spmap] spawned received Pure Heart %d\n", sDeliveringHeart);
        return;
    }

    // A Progressive Wallet isn't a game item: it plays as the Archipelago item's item-get with the
    // wallet's picture, and its size goes up once it's picked up (deliveryDone)
    if (!local && itemId == WALLET_ITEM_ID)
    {
        archipelago_item::armWallet();
        sDeliveringWallet = true;
        spawnOnMario(archipelago_item::AP_ITEM_ID);
        wii::os::OSReport("[spmap] spawned received wallet\n");
        return;
    }

    // Characters crash the world-item pickup (they're never world items in vanilla), so they're
    // added straight to the party; the inventory hook below finishes the delivery
    if (itemId == ITEM_ID_CHAR_START)
    {
        // Mario (starting_character): he's out of the party until his item, which puts him back
        starting_character::giveMario();
        deliveryDone();
        return;
    }
    if (itemId >= ITEM_ID_CHAR_START && itemId < ITEM_ID_CHAR_MAX)
    {
        sDeliveringItem = itemId;
        spm::mario_pouch::pouchAddItem(itemId);
        return;
    }

    spawnOnMario(itemId);
    wii::os::OSReport("[spmap] spawned received item %d\n", itemId);

    // World pickups (coins, flowers, world mushrooms) take effect without going through the
    // inventory, so the hook never sees them: they count as delivered once they appear
    if (itemId < ITEM_ID_KEY_START)
        deliveryDone();
}

void giveLocal(s32 itemId)
{
    if (sLocalCount == LOCAL_QUEUE_SIZE)
    {
        wii::os::OSReport("[spmap] local item queue full, dropped item %d\n", itemId);
        return;
    }
    sLocalQueue[sLocalCount++] = itemId;
    wii::os::OSReport("[spmap] queued local item %d\n", itemId);
}

void onMapChange()
{
    if (sDeliveringItem != NO_ITEM)
    {
        wii::os::OSReport("[spmap] received item %d not collected before map change, will respawn\n", sDeliveringItem);
        sDeliveringItem = NO_ITEM;
        sDeliveringWallet = false;
        sDeliveringHeart = 0;
    }
}

void onNoSaveLoaded()
{
    mailbox::Mailbox & mb = mailbox::gMailbox;
    mb.savedRecvCount = 0xFFFFFFFF;
    mb.recvHandled = mb.recvCount;
    sDeliveringItem = NO_ITEM;
    sLocalCount = 0;
}

bool isBusy()
{
    s32 waiting;
    return sDeliveringItem != NO_ITEM || sCooldown > 0 || sLocalCount > 0 || mailbox::peekReceived(&waiting);
}

// ---- The inventory hook ----

// Whether an item id is a key item.
static bool isKeyItem(s32 itemId)
{
    return itemId >= ITEM_ID_KEY_START && itemId < ITEM_ID_KEY_MAX;
}

// Whether an item id is a Pixl.
static bool isPixl(s32 itemId)
{
    return itemId >= ITEM_ID_FAIRY_START && itemId < ITEM_ID_FAIRY_MAX;
}

/*
    Runs instead of the game's pouchAddItem, which every collected item goes through:
      - finishes a delivery when the delivered item arrives
      - the Archipelago item never goes into the inventory
      - a grouped key goes in as its chapter's key (key_groups)
      - a key item with all 32 slots full goes to storage instead
      - a first Pixl gets selected (the game's function doesn't select it)
      - Mario joining any way (his item found in a level, not only delivered) counts as held
*/
static bool (*pouchAddItemReal)(s32 itemId);
static bool pouchAddItemHook(s32 itemId)
{
    if (sDeliveringItem != NO_ITEM && itemId == sDeliveringItem)
    {
        deliveryDone();
        wii::os::OSReport("[spmap] delivered received item %d\n", itemId);
    }
    else
    {
        mailbox::sendLocation(PICKUP_LOG_LOCATION_BASE + itemId);
        wii::os::OSReport("[spmap] picked up item %d\n", itemId);
    }

    if (itemId == archipelago_item::AP_ITEM_ID)
        return true;
    itemId = key_groups::canonical(itemId);  // a grouped key becomes its chapter's key
    if (isKeyItem(itemId) && key_item_storage::keyItemsFull() && key_item_storage::store(itemId))
        return true;

    bool added = pouchAddItemReal(itemId);
    if (isPixl(itemId))
        pixl_joins::repairPixlSelection();
    if (itemId == ITEM_ID_CHAR_START)
        starting_character::onMarioAdded();
    return added;
}

void init()
{
    pouchAddItemReal = game_mode::hook(spm::mario_pouch::pouchAddItem, pouchAddItemHook);
}

}
