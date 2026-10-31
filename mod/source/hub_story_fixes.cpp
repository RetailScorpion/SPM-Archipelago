#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "hub_story_fixes.h"
#include "story_progress.h"

namespace mod::hub_story_fixes {

#define CHAPTER_6_DUELS_DONE 280   // Sammer's Kingdom: the story's duels are done
#define CHAPTER_6_WORLD_ENDED 281  // what vanilla's Flipside Tower scene sets after them

#define CASTLE_BLECK_DOOR_MAP "mac_12"  // Flopside Tower
#define CASTLE_BLECK_DOOR_VALUE 357     // its door leads to Chapter 8 for story values 356-358

// Whether Chapters 1 to `last` are all finished.
static bool chaptersFinished(s32 last)
{
    for (s32 chapter = 1; chapter <= last; chapter++)
    {
        if (!story_progress::isChapterFinished(chapter))
            return false;
    }
    return true;
}

// Whether Mario is entering Flopside Tower.
static bool enteringFlopsideTower(const char * nextMap)
{
    return story_progress::gStory.context == STORY_CONTEXT_HUB && nextMap != nullptr
        && msl::string::strcmp(nextMap, CASTLE_BLECK_DOOR_MAP) == 0;
}

void onMapChange(const char * nextMap)
{
    if (story_progress::getChapterValue(6) == CHAPTER_6_DUELS_DONE)
        story_progress::advanceChapter(6, CHAPTER_6_WORLD_ENDED);

    if (enteringFlopsideTower(nextMap) && chaptersFinished(7))
    {
        spm::spmario::gp->gsw0 = CASTLE_BLECK_DOOR_VALUE;
        wii::os::OSReport("[spmap] hubstory: Chapters 1-7 finished, Castle Bleck door open in %s\n", nextMap);
    }
}

}
