#include <common.h>
#include <spm/evt_sub.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "returnpipe.h"

namespace mod::returnpipe {

using spm::evtmgr::EvtScriptCode;

#define OP_USER_FUNC 92

// USER_FUNC(func_800d4de4, 1, 1) in Merlee's Mansion (offsets from relF_section5)
static const struct { u32 offset; const char * map; } sMansionDisables[] = {
    {0x60e00, "mi3_01"}, {0x61930, "mi3_02"}, {0x62f1c, "mi3_03"},
    {0x641fc, "mi3_04"}, {0x65074, "mi3_05"}, {0x6602c, "mi3_06"},
};

void init()
{
    for (auto & d : sMansionDisables)
    {
        EvtScriptCode * at = gamesyms::relF_section5 + d.offset / sizeof(EvtScriptCode);
        if (at[0] != ((3 << 16) | OP_USER_FUNC) || at[1] != reinterpret_cast<s32>(spm::evt_sub::func_800d4de4)
            || at[2] != 1 || at[3] != 1)
        {
            wii::os::OSReport("[spmap] returnpipe: %s isn't what was expected, not patched\n", d.map);
            continue;
        }
        at[3] = 0;  // enable instead of disable
    }
    wii::os::OSReport("[spmap] returnpipe: mansion patched\n");
}

}
