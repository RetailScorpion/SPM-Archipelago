#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "game_addresses.h"
#include "save_data.h"
#include "story_progress.h"
#include "tippi_always_on.h"

namespace mod::tippi_always_on {

// In Tippi's flags: set = she's switched off
#define TIPPI_OFF_FLAG 0x80

// The prologue: until its pillar scene (story value 9), the Tippi on screen is a look-alike
// character, not the real one
#define PROLOGUE_TIPPI_END 9

// Tippi's flags (the first 2 bytes of her state).
static u16 * tippiFlags()
{
    return static_cast<u16 *>(game_addresses::guideGetWork());
}

void onMapChange(s32 context, s32 storyValue)
{
    if (context == STORY_CONTEXT_NONE || storyValue < PROLOGUE_TIPPI_END)
        return;

    // The save flag makes loading a save restore her; clearing her "off" flag covers this map
    spm::swdrv::swSet(GSWF_TIPPI_PRESENT);
    u16 * flags = tippiFlags();
    if (*flags & TIPPI_OFF_FLAG)
    {
        *flags &= ~TIPPI_OFF_FLAG;
        wii::os::OSReport("[spmap] tippi: switched back on (context %d, value %d)\n", context, storyValue);
    }
}

}
