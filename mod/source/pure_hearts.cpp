#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "archipelago_item.h"
#include "mailbox.h"
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

bool isDoorOpen(s32 heart)
{
    return mailbox::gMailbox.startingHearts == STARTING_HEARTS_NO_CLIENT || spm::swdrv::swGet(AP_GSWF_HEART(heart));
}

// Whether the seed starts with Pure Heart `heart` (bit heart - 1 of startingHearts).
static bool startsWithHeart(u32 startingHearts, s32 heart)
{
    return startingHearts & (1 << (heart - 1));
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
    if (placed == 0)
        giveHeart(chapter + 1);  // no client: the vanilla heart
    else if (placed == JOIN_ITEM_OTHER_WORLD)
        received_items::giveLocal(archipelago_item::AP_ITEM_ID);
    else
        received_items::giveLocal(placed);
}

void onMapChange()
{
    for (s32 chapter = 1; chapter <= HEART_CHAPTERS; chapter++)
        collectChapterHeart(chapter);

    u32 starting = mailbox::gMailbox.startingHearts;
    if (starting == STARTING_HEARTS_NO_CLIENT)
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
