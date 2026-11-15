#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "archipelago_item.h"
#include "mailbox.h"
#include "offline_checks.h"
#include "pure_hearts.h"
#include "received_items.h"
#include "save_data.h"
#include "story_progress.h"

namespace mod::pure_hearts {

bool isHeartItem(s32 itemId)
{
    return itemId >= PURE_HEART_ITEM_FIRST && itemId < PURE_HEART_ITEM_FIRST + PURE_HEART_COUNT;
}

void giveHeart(s32 heart)
{
    spm::swdrv::swSet(AP_GSWF_HEART(heart));
    wii::os::OSReport("[spmap] doors: Pure Heart %d received\n", heart);
}

// Whether door `heart` is open from the start (bit heart - 1 of startingHearts).
static bool startsWithHeart(u32 startingHearts, s32 heart)
{
    return startingHearts & (1 << (heart - 1));
}

/*
    The seed's starting doors: the client's, kept in the save as they come (user, 2026-11-12: a save
    loaded in Flipside Tower before the client connected had its starting doors shut until the room
    was entered again), or the save's from the client's last connection. STARTING_HEARTS_NO_CLIENT
    for a file the client has never connected to.
*/
static u32 startingDoors()
{
    u32 starting = mailbox::gMailbox.startingHearts;
    if (starting != STARTING_HEARTS_NO_CLIENT)
    {
        u8 saved = static_cast<u8>((starting & ~STARTING_DOORS_SAVED) | STARTING_DOORS_SAVED);
        if (save_data::getGswByte(AP_GSW_STARTING_DOORS) != saved)
            save_data::setGswByte(AP_GSW_STARTING_DOORS, saved);
        return starting;
    }
    u8 saved = save_data::getGswByte(AP_GSW_STARTING_DOORS);
    return saved & STARTING_DOORS_SAVED ? saved & ~STARTING_DOORS_SAVED : STARTING_HEARTS_NO_CLIENT;
}

bool isDoorOpen(s32 heart)
{
    u32 starting = startingDoors();
    // With no seed (the client has never connected to this file), no door is open from the start
    // (user, 2026-11-08): only the hearts the file has received open theirs
    if (starting != STARTING_HEARTS_NO_CLIENT && startsWithHeart(starting, heart))
        return true;
    return spm::swdrv::swGet(AP_GSWF_HEART(heart));
}

#define HEART_CHAPTERS 7  // Chapters 1-7 each end with a Pure Heart

// Collects a finished chapter's Pure Heart location: its flag, then what the seed placed there.
static void collectChapterHeart(s32 chapter)
{
    s32 flag = AP_GSWF_CHAPTER_HEART(chapter);
    if (spm::swdrv::swGet(flag) || !story_progress::isChapterFinished(chapter))
        return;
    // The flag first: it marks the location collected and names an Archipelago item's item-get
    spm::swdrv::swSet(flag);
    u16 placed = mailbox::gMailbox.heartLocItems[chapter - 1];
    wii::os::OSReport("[spmap] doors: Chapter %d's Pure Heart location collected (%d)\n", chapter, placed);
    if (offline_checks::active())
        received_items::giveLocal(archipelago_item::AP_ITEM_ID);  // logged; its item comes once connected
    else if (placed == 0)
        giveHeart(chapter + 1);  // the vanilla heart
    else if (placed == JOIN_ITEM_OTHER_WORLD)
        received_items::giveLocal(archipelago_item::AP_ITEM_ID);
    else
        received_items::giveLocal(placed);
}

void onMapChange()
{
    startingDoors();  // keeps the save's copy up to date
    for (s32 chapter = 1; chapter <= HEART_CHAPTERS; chapter++)
        collectChapterHeart(chapter);

    u32 starting = mailbox::gMailbox.startingHearts;
    // Starting doors' hearts that are items to find aren't given (their doors open anyway)
    if (starting == STARTING_HEARTS_NO_CLIENT || mailbox::gMailbox.startingHeartsInPool)
        return;
    for (s32 heart = 1; heart <= PURE_HEART_COUNT; heart++)
    {
        if (startsWithHeart(starting, heart) && !spm::swdrv::swGet(AP_GSWF_HEART(heart)))
        {
            spm::swdrv::swSet(AP_GSWF_HEART(heart));
            wii::os::OSReport("[spmap] doors: starting with Pure Heart %d\n", heart);
        }
    }
}

}
