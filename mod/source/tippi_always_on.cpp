#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "code_patching.h"

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

// Tippi's pointer: "cmpwi r3, 12" on the story value (no pointing below it) becomes 9
#define POINTER_CHECK_VANILLA 0x2C03000C  // cmpwi r3, 12
#define POINTER_CHECK_PATCHED (0x2C030000 | PROLOGUE_TIPPI_END)

void init()
{
    u32 * at = game_addresses::tippi_pointer_story_check;
    if (*at != POINTER_CHECK_VANILLA)
    {
        wii::os::OSReport("[spmap] tippi: pointer check isn't what was expected (%08x), not patched\n", *at);
        return;
    }
    code_patching::writeInstruction(at, POINTER_CHECK_PATCHED);
    wii::os::OSReport("[spmap] tippi: pointing allowed from story value %d\n", PROLOGUE_TIPPI_END);
}

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
