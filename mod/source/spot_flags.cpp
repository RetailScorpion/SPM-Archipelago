#include <common.h>
#include <wii/os.h>

#include "save_data.h"
#include "script_patching.h"
#include "spot_flags.h"

namespace mod::spot_flags {

using namespace mod::script_patching;

// evt_item_entry(name, item, behaviour, x, y, z, ?, pickup script, flag, ?): 10 arguments after the
// function, with the flag the 9th
#define ITEM_ENTRY_WORDS EVT_CMD(11, OP_USER_FUNC)
#define ITEM_ENTRY_FLAG_WORD 10  // words from the command's start

static const struct
{
    u32 offset;  // the evt_item_entry command: bytes from the start of relF's script section
    s32 item;    // the item it spawns in vanilla (checked, then left to the client)
    const char * what;
} sSpots[] = {
    // King Sammer's rewards (wa4_26)
    {0xe3b78, 496, "King Sammer: Goombella Card"},
    {0xe3bc8, 497, "King Sammer: Koops Card"},
    {0xe3c18, 498, "King Sammer: Madame Flurrie Card"},
    {0xe3c68, 499, "King Sammer: Yoshi Card"},
    {0xe3cb8, 500, "King Sammer: Vivian Card"},
    {0xe3d08, 501, "King Sammer: Admiral Bobbery Card"},
    {0xe3d58, 502, "King Sammer: Ms. Mowz Card"},
};

void init()
{
    static const s32 command[] = {ITEM_ENTRY_WORDS};
    static const s32 noFlag[] = {0};
    for (u32 n = 0; n < sizeof(sSpots) / sizeof(sSpots[0]); n++)
    {
        EvtScriptCode * at = relfScript(sSpots[n].offset);
        // The command and its item; the flag must still be 0
        if (!matches(at, command, 1, sSpots[n].what) || !matches(at + 3, &sSpots[n].item, 1, sSpots[n].what) ||
            !matches(at + ITEM_ENTRY_FLAG_WORD, noFlag, 1, sSpots[n].what))
            continue;
        at[ITEM_ENTRY_FLAG_WORD] = GSWF_VAR(AP_GSWF_LOC_BASE + n);
    }
    wii::os::OSReport("[spmap] spots: flags given to unflagged item spots\n");
}

}
