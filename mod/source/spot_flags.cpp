#include <common.h>
#include <wii/os.h>

#include "game_mode.h"
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
    // Chapter 7's Door Keys (user, 2026-11-05; gen_ap_data.py LATE_SPOTS, same order)
    {0xea760, 49, "7-1 Door Key (an1_04)"},
    {0xf26bc, 37, "7-2 Door Key (an2_03, the D-Men)"},
    {0xf79ec, 38, "7-2 Door Key (an2_07, the sisters)"},
    {0x10d0a8, 40, "7-4 Door Key (an4_03)"},
    {0x10e3e0, 40, "7-4 Door Key (an4_04)"},
    // 7-4's orbs (user, 2026-11-05)
    {0x10ee8c, 41, "7-4 Blue Orb (an4_04)"},
    {0x1111d8, 42, "7-4 Yellow Orb (an4_06)"},
    {0x111fe8, 43, "7-4 Red Orb (an4_07)"},
    // Every other key (user, 2026-11-07; gen_ap_data.py LATE_SPOTS, same order)
    {0x4a944, 16, "1-4 Ruins Key (he4_04, box)"},
    {0x4b0a0, 16, "1-4 Ruins Key (he4_05)"},
    {0x4b958, 16, "1-4 Ruins Key (he4_07)"},
    {0x5319c, 17, "2-1 Door Key (mi1_05)"},
    {0x5b76c, 18, "2-2 House Key (mi2_06, box)"},
    {0x888dc, 19, "3-4 Fort Key (ta4_02, box)"},
    {0x9c498, 26, "4-2 Door Key (sp2_01)"},
    {0xa7134, 27, "4-4 Dimension Key (sp4_04, box)"},
    {0xa758c, 28, "4-4 Dimension Key (sp4_07, box)"},
    {0xa8494, 29, "4-4 Dimension Key (sp4_12, box)"},
    {0xbe160, 33, "5-3 Cave Key (gn3_01)"},
    {0xc76cc, 34, "5-4 Cave Key (gn4_06)"},
    {0xc7e30, 35, "5-4 Card Key (gn4_10)"},
    {0x11eff4, 46, "8-1 Castle Bleck Key (ls1_07, box)"},
    {0x126d64, 46, "8-2 Castle Bleck Key (ls2_02, box)"},
    {0x12c9e0, 46, "8-2 Castle Bleck Key (ls2_12)"},
    {0x132284, 46, "8-3 Castle Bleck Key (ls3_03, block)"},
    {0x133b94, 46, "8-3 Castle Bleck Key (ls3_09, box)"},
    {0x134574, 46, "8-3 Castle Bleck Key (ls3_11, box)"},
    {0x139350, 46, "8-4 Castle Bleck Key (ls4_05, box)"},
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
        game_mode::set<s32>(&at[ITEM_ENTRY_FLAG_WORD], GSWF_VAR(AP_GSWF_LOC_BASE + n));
    }
    wii::os::OSReport("[spmap] spots: flags given to unflagged item spots\n");
}

}
