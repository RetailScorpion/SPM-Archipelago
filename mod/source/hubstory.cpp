#include <common.h>
#include <msl/string.h>
#include <spm/item_data_ids.h>
#include <spm/mario_pouch.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "delivery.h"
#include "hubstory.h"
#include "storyvalue.h"

namespace mod::hubstory {

#define ITEM_PEACH 217
#define ITEM_LUIGI 219
#define ITEM_HELMET 24

#define CHAPTER_6_DUELS_DONE 280   // Sammer's Kingdom, end of the story's duels
#define CHAPTER_6_WORLD_ENDED 281  // set by the Flipside Tower scene after them

#define CASTLE_BLECK_DOOR_MAP "mac_12"  // Flopside Tower
#define CASTLE_BLECK_DOOR_VALUE 357     // its door doa6_l leads to Chapter 8 for 356-358

// Items queued this session, so a map change before delivery doesn't queue them again
static bool sQueuedPeach, sQueuedLuigi, sQueuedHelmet;

static bool hasCharacter(s32 itemId)
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->characters)
    {
        if (info.itemType == itemId)
            return true;
    }
    return false;
}

static void give(s32 itemId, bool & queued, const char * why)
{
    if (queued)
        return;
    queued = true;
    delivery::giveLocal(itemId);
    wii::os::OSReport("[spmap] hubstory: giving item %d (%s)\n", itemId, why);
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

    if (storyvalue::isChapterFinished(1) && !hasCharacter(ITEM_PEACH))
        give(ITEM_PEACH, sQueuedPeach, "Chapter 1 finished");
    if (context == 7 && !hasCharacter(ITEM_LUIGI))
        give(ITEM_LUIGI, sQueuedLuigi, "entered Chapter 7");
    if (context == 4 && !spm::mario_pouch::pouchCheckHaveItem(ITEM_HELMET))
        give(ITEM_HELMET, sQueuedHelmet, "entered Chapter 4");

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
