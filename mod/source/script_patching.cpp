#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_door.h>
#include <spm/evt_map.h>
#include <spm/evt_pouch.h>
#include <spm/evt_snd.h>
#include <wii/os.h>

#include "game_addresses.h"
#include "script_patching.h"

namespace mod::script_patching {

// The functions skipCall can switch off, in SkippableCall order
static const void * const sSkippableCalls[] = {
    reinterpret_cast<const void *>(spm::evt_door::evt_door_enable_disable_map_door_desc),
    reinterpret_cast<const void *>(spm::evt_snd::evt_snd_sfxon_3d),
    reinterpret_cast<const void *>(spm::evt_map::evt_map_playanim),
    reinterpret_cast<const void *>(spm::evt_pouch::evt_pouch_add_item),
};

// A script function that does nothing and finishes at once (2 = "done" to the script engine).
static EVT_DEFINE_USER_FUNC(ap_skip)
{
    (void) evt;
    (void) isFirstCall;
    return 2;
}

EvtScriptCode * relfScript(u32 offset)
{
    return game_addresses::relF_section5 + offset / sizeof(EvtScriptCode);
}

bool matches(const EvtScriptCode * at, const s32 * expected, s32 count, const char * what)
{
    for (s32 i = 0; i < count; i++)
    {
        if (at[i] != expected[i])
        {
            wii::os::OSReport("[spmap] script patch: %s isn't what was expected, not patched\n", what);
            return false;
        }
    }
    return true;
}

void caseComparesWithSaveByte(EvtScriptCode * at, s32 value, s32 saveByte, const char * what)
{
    const s32 expected[] = {EVT_CMD(1, OP_CASE_SMALL), value};
    if (matches(at, expected, 2, what))
        at[1] = GSW_VAR(saveByte);
}

void skipCall(EvtScriptCode * at, s32 argCount, SkippableCall call, const char * what)
{
    const s32 expected[] = {EVT_CMD(argCount + 1, OP_USER_FUNC), reinterpret_cast<s32>(sSkippableCalls[call])};
    if (matches(at, expected, 2, what))
        at[1] = reinterpret_cast<s32>(ap_skip);
}

}
