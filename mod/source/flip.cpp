#include <common.h>
#include <spm/evtmgr.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "flip.h"
#include "gamesyms.h"
#include "patch.h"
#include "savedata.h"
#include "storyvalue.h"

namespace mod::flip {

// Story value Bestovius sets when he teaches flipping
#define STORY_FLIP_LEARNED 16

// TODO: from slot data
static FlipMode sMode = FLIP_START_WITH;

bool isUnlocked()
{
    switch (sMode)
    {
        case FLIP_START_WITH:
            return true;
        case FLIP_VANILLA:
            return storyvalue::getChapterValue(1) >= STORY_FLIP_LEARNED;
        case FLIP_RANDOM:
            return spm::swdrv::swGet(AP_GSWF_FLIP);
    }
    return true;
}

/*
    Replaces evtGetValue(nullptr, GSW(0)) in the ability check, which blocks flipping when the
    result is 1-15. Returning 0 allows it, 1 blocks it.
*/
static s32 flipStoryCheck(spm::evtmgr::EvtEntry * entry, s32 var)
{
    (void) entry;
    (void) var;
    return isUnlocked() ? 0 : 1;
}

void init()
{
    // Expect "bl evtGetValue" here (0x48xxxxx1)
    u32 ins = gamesyms::mario_flip_story_check[0];
    if ((ins & 0xFC000003) != 0x48000001)
    {
        wii::os::OSReport("[spmap] flip: unexpected instruction %08x, not patched\n", ins);
        return;
    }
    patch::writeBranch(gamesyms::mario_flip_story_check, reinterpret_cast<void *>(flipStoryCheck), true);
    wii::os::OSReport("[spmap] flip: story check replaced\n");
}

}
