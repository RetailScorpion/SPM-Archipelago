#include <common.h>
#include <spm/evtmgr.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "code_patching.h"
#include "flip_ability.h"
#include "game_addresses.h"
#include "save_data.h"
#include "story_progress.h"

namespace mod::flip_ability {

// The story value Bestovius sets when he teaches flipping
#define STORY_FLIP_LEARNED 16

// PowerPC "bl" (call) instruction: the top 6 bits are 18 and the lowest bit is 1
#define CALL_INSTRUCTION_MASK 0xFC000003
#define CALL_INSTRUCTION 0x48000001

// Fixed until the client sends the flip option
static FlipMode sMode = FLIP_START_WITH;

bool isUnlocked()
{
    switch (sMode)
    {
        case FLIP_START_WITH:
            return true;
        case FLIP_VANILLA:
            return story_progress::getChapterValue(1) >= STORY_FLIP_LEARNED;
        case FLIP_RANDOM:
            return spm::swdrv::swGet(AP_GSWF_FLIP);
    }
    return true;
}

// Runs where the game read the story value (evtGetValue on GSW 0). The game blocks the flip when
// that value is 1-15, so 0 allows it and 1 blocks it.
static s32 flipStoryCheck(spm::evtmgr::EvtEntry * entry, s32 var)
{
    (void) entry;
    (void) var;
    return isUnlocked() ? 0 : 1;
}

void init()
{
    u32 instruction = game_addresses::mario_flip_story_check[0];
    if ((instruction & CALL_INSTRUCTION_MASK) != CALL_INSTRUCTION)
    {
        wii::os::OSReport("[spmap] flip: unexpected instruction %08x, not patched\n", instruction);
        return;
    }
    code_patching::writeBranch(game_addresses::mario_flip_story_check, reinterpret_cast<void *>(flipStoryCheck), true);
    wii::os::OSReport("[spmap] flip: story check replaced\n");
}

}
