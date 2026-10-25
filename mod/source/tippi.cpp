#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "savedata.h"
#include "storyvalue.h"
#include "tippi.h"

namespace mod::tippi {

#define GUIDE_FLAG0_DISABLE 0x80

static u16 * guideFlag0()
{
    return static_cast<u16 *>(gamesyms::guideGetWork());
}

// Where the story's Tippi isn't the real guide yet. The long stretches without her in vanilla
// (Chapter 3 after Francis swallows her, the Underwhere after Chapter 6) are deliberately not
// listed: the randomizer keeps her through them, since pointing is needed everywhere.
static bool storyRemovesTippi(s32 context, s32 value)
{
    (void) context;
    return value < 9;  // prologue: she's an NPC stand-in until the pillar scene
}

void onMapChange(s32 context, s32 storyValue)
{
    if (context == STORY_CONTEXT_NONE || storyRemovesTippi(context, storyValue))
        return;

    // The flag makes the load code restore her; clearing the bit covers ordinary map changes
    spm::swdrv::swSet(GSWF_TIPPI_PRESENT);
    u16 * flag0 = guideFlag0();
    if (*flag0 & GUIDE_FLAG0_DISABLE)
    {
        *flag0 &= ~GUIDE_FLAG0_DISABLE;
        wii::os::OSReport("[spmap] tippi: switched back on (context %d, value %d)\n", context, storyValue);
    }
}

}
