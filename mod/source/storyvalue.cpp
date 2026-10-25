#include <common.h>
#include <spm/spmario.h>
#include <wii/os.h>
#include <msl/string.h>

#include "storyvalue.h"

namespace mod::storyvalue {

// Hub value for the experiment: after the Pure Heart is placed in the Flipside 1F Outskirts pillar
// (Peach revived), so the Lineland and Gloam Valley doors are both open and no hub cutscene is pending
#define HUB_VALUE 65

// Value each chapter starts at: what the game sets when you go through that chapter's door
static const s32 sChapterStart[STORY_SLOT_COUNT] = {
    HUB_VALUE, 11, 66, 101, 129, 179, 225, 303, 359,
};

// Map name prefix -> context. Chapter 7 (an) also hosts the Underwhere intermission at the end of
// Chapter 6; that's ignored for this experiment.
static const struct { const char * prefix; s32 context; } sPrefixes[] = {
    {"mac_", STORY_CONTEXT_HUB},
    {"he", 1}, {"mi", 2}, {"ta", 3}, {"sp", 4}, {"gn", 5}, {"wa", 6}, {"an", 7}, {"ls", 8},
};

StoryState gStory;

void init()
{
    gStory.context = STORY_CONTEXT_NONE;
    for (s32 i = 0; i < STORY_SLOT_COUNT; i++)
        gStory.slots[i] = sChapterStart[i];
}

s32 contextForMap(const char * map)
{
    for (auto & p : sPrefixes)
    {
        if (msl::string::strncmp(map, p.prefix, msl::string::strlen(p.prefix)) == 0)
            return p.context;
    }
    return STORY_CONTEXT_NONE;
}

void onMapChange(const char * nextMap)
{
    s32 & gsw0 = spm::spmario::gp->gsw0;
    s32 next = contextForMap(nextMap);

    // Save progress made in the chapter we're leaving. The hub value is fixed, so anything a hub
    // script wrote (like the "entered the door" values) is dropped.
    if (gStory.context > STORY_CONTEXT_HUB)
        gStory.slots[gStory.context] = gsw0;
    else if (gStory.context == STORY_CONTEXT_HUB && gsw0 != gStory.slots[STORY_CONTEXT_HUB])
        wii::os::OSReport("[spmap] story: hub wrote %d, discarded\n", gsw0);

    s32 old = gsw0;
    if (next != STORY_CONTEXT_NONE)
        gsw0 = gStory.slots[next];

    wii::os::OSReport("[spmap] story: map %s, context %d -> %d, value %d -> %d\n",
                      nextMap, gStory.context, next, old, gsw0);
    gStory.context = next;
}

}
