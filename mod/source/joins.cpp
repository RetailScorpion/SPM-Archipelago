#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_pouch.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario_pouch.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "joins.h"
#include "savedata.h"

namespace mod::joins {

using spm::evtmgr::EvtScriptCode;

// evt opcodes and variable encodings (see evt_cmd.h)
#define OP_IF_EQUAL 24
#define OP_IF_SMALL 26
#define OP_USER_FUNC 92
#define EVT_CMD(argc, op) (((argc) << 16) | (op))
#define GSW0_VAR -170000000
#define GSWF_VAR(id) (-130000000 + (id))
#define LW_VAR(id) (-30000000 + (id))

// ap_join_gift(pixlItemId): a Pixl join location. Marks it collected; until Archipelago is
// connected it still gives the vanilla Pixl.
static EVT_DEFINE_USER_FUNC(ap_join_gift)
{
    (void) isFirstCall;
    s32 item = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    spm::swdrv::swSet(AP_GSWF_JOIN(item));
    spm::mario_pouch::pouchAddItem(item);
    wii::os::OSReport("[spmap] joins: location for Pixl %d collected\n", item);
    return 2;
}

static bool expect(const EvtScriptCode * at, const s32 * words, s32 n, const char * what)
{
    for (s32 i = 0; i < n; i++)
    {
        if (at[i] != words[i])
        {
            wii::os::OSReport("[spmap] joins: %s isn't what was expected, not patched\n", what);
            return false;
        }
    }
    return true;
}

// IF_SMALL(GSW(0), storyValue)  ->  IF_EQUAL(GSWF(flag), 0)
static void patchGate(EvtScriptCode * at, s32 storyValue, s32 flag, const char * what)
{
    const s32 orig[] = {EVT_CMD(2, OP_IF_SMALL), GSW0_VAR, storyValue};
    if (!expect(at, orig, 3, what))
        return;
    at[0] = EVT_CMD(2, OP_IF_EQUAL);
    at[1] = GSWF_VAR(flag);
    at[2] = 0;
}

// IF_SMALL(GSW(0), storyValue)  ->  IF_SMALL(GSW(0), 0): never true, so the block never runs
static void patchNever(EvtScriptCode * at, s32 storyValue, const char * what)
{
    const s32 orig[] = {EVT_CMD(2, OP_IF_SMALL), GSW0_VAR, storyValue};
    if (!expect(at, orig, 3, what))
        return;
    at[2] = 0;
}

// USER_FUNC(evt_pouch_add_item, item)  ->  USER_FUNC(ap_join_gift, item)
static void patchGift(EvtScriptCode * at, s32 item, const char * what)
{
    const s32 orig[] = {EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_pouch::evt_pouch_add_item), item};
    if (!expect(at, orig, 3, what))
        return;
    at[1] = reinterpret_cast<s32>(ap_join_gift);
}

// IF_EQUAL(LW(0), 0) guarding a safety net (LW(0) = "has the Pixl")  ->  never true
static void patchSafetyNet(EvtScriptCode * at, const char * what)
{
    const s32 orig[] = {EVT_CMD(2, OP_IF_EQUAL), LW_VAR(0), 0};
    if (!expect(at, orig, 3, what))
        return;
    at[2] = -1;
}

void init()
{
    using namespace mod::gamesyms;
    const s32 thoreau = 221;
    patchGate(he2_07_thoreau_gate_spawn, 25, AP_GSWF_JOIN(thoreau), "Thoreau spawn gate");
    // The room's trap (iron bars and a locked exit) expects Thoreau to get you out; the chest may
    // hold anything in the randomizer, so the trap never happens
    patchNever(he2_07_thoreau_gate_lock, 25, "Thoreau lock gate");
    patchNever(he2_07_thoreau_gate_bars, 25, "Thoreau bars gate");
    patchGift(he2_07_thoreau_join_gift, thoreau, "Thoreau join gift");
    patchSafetyNet(he2_03_thoreau_safety_net, "Thoreau safety net");
    wii::os::OSReport("[spmap] joins: patched\n");
}

}
