#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "apitem.h"
#include "delivery.h"
#include "hubstory.h"
#include "mailbox.h"
#include "savedata.h"
#include "storyvalue.h"

namespace mod::hubstory {

#define CHAPTER_6_DUELS_DONE 280   // Sammer's Kingdom, end of the story's duels
#define CHAPTER_6_WORLD_ENDED 281  // set by the Flipside Tower scene after them

#define CASTLE_BLECK_DOOR_MAP "mac_12"  // Flopside Tower
#define CASTLE_BLECK_DOOR_VALUE 357     // its door doa6_l leads to Chapter 8 for 356-358

/*
    Story gift locations. Each is collected once, when its trigger first holds: its own save flag
    (AP_GSWF_GIFT) is the "collected" state, like a Pixl join's, so nothing depends on story values.
    It gives what the client placed there (mailbox giftItems), or its vanilla item without a client.
*/
enum Trigger
{
    CHAPTER_1_FINISHED,
    ENTERED_CHAPTER_4,
    ENTERED_CHAPTER_7,
};

static const struct
{
    s32 vanillaItem;
    Trigger trigger;
    const char * what;
} sGifts[] = {
    {217, CHAPTER_1_FINISHED, "Peach joins"},   // vanilla: Flipside Tower scene, story value 63
    {219, ENTERED_CHAPTER_7, "Luigi joins"},    // vanilla: the Underwhere, story value 297
    {24, ENTERED_CHAPTER_4, "the Helmet"},      // vanilla: Merlon's Goldfish Bowl quest, 131-134
};

static bool triggered(Trigger trigger, s32 context)
{
    switch (trigger)
    {
        case CHAPTER_1_FINISHED:
            return storyvalue::isChapterFinished(1);
        case ENTERED_CHAPTER_4:
            return context == 4;
        case ENTERED_CHAPTER_7:
            return context == 7;
    }
    return false;
}

static void collectGifts(s32 context)
{
    for (u32 i = 0; i < sizeof(sGifts) / sizeof(sGifts[0]); i++)
    {
        s32 flag = AP_GSWF_GIFT(i);
        if (spm::swdrv::swGet(flag) || !triggered(sGifts[i].trigger, context))
            continue;
        // The flag first: it marks the location collected (the client reports it) and, for another
        // world's item, tells the AP item-get whose names to show
        spm::swdrv::swSet(flag);
        u16 placed = mailbox::gMailbox.giftItems[i];
        s32 item = placed == 0 ? sGifts[i].vanillaItem : placed == JOIN_ITEM_OTHER_WORLD ? apitem::AP_ITEM_ID : placed;
        delivery::giveLocal(item);
        wii::os::OSReport("[spmap] hubstory: %s location collected, giving item %d\n", sGifts[i].what, item);
    }
}

static bool chaptersFinished(s32 last)
{
    for (s32 chapter = 1; chapter <= last; chapter++)
    {
        if (!storyvalue::isChapterFinished(chapter))
            return false;
    }
    return true;
}

void onMapChange(const char * nextMap)
{
    s32 context = storyvalue::gStory.context;

    collectGifts(context);

    if (storyvalue::getChapterValue(6) == CHAPTER_6_DUELS_DONE)
        storyvalue::advanceChapter(6, CHAPTER_6_WORLD_ENDED);

    if (context == STORY_CONTEXT_HUB && nextMap != nullptr
        && msl::string::strcmp(nextMap, CASTLE_BLECK_DOOR_MAP) == 0 && chaptersFinished(7))
    {
        spm::spmario::gp->gsw0 = CASTLE_BLECK_DOOR_VALUE;
        wii::os::OSReport("[spmap] hubstory: Chapters 1-7 finished, Castle Bleck door open in %s\n", nextMap);
    }
}

}
