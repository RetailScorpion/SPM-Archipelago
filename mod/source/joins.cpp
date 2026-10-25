#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_pouch.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario_pouch.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "joins.h"
#include "mailbox.h"
#include "savedata.h"
#include "storyvalue.h"

namespace mod::joins {

using spm::evtmgr::EvtScriptCode;

// evt opcodes and variable encodings (see evt_cmd.h)
#define OP_IF_EQUAL 24
#define OP_IF_NOT_EQUAL 25
#define OP_IF_SMALL 26
#define OP_SWITCH 34
#define OP_CASE_SMALL 38
#define OP_USER_FUNC 92
#define EVT_CMD(argc, op) (((argc) << 16) | (op))
#define GSW_VAR(id) (-170000000 + (id))
#define GSWF_VAR(id) (-130000000 + (id))
#define LW_VAR(id) (-30000000 + (id))

#define PIXL_THOREAU 221
#define PIXL_BOOMER 222
#define PIXL_SLIM 223
#define PIXL_THUDLEY 224
#define PIXL_CARRIE 225
#define PIXL_FLEEP 226
#define PIXL_CUDGE 227
#define PIXL_DOTTIE 228

// Stage value meaning "join done", past every vanilla stage
#define STAGE_DONE 200

/*
    Stages: a patched SWITCH(GSW(0)) reads AP_GSW_JOIN_STAGES + index instead
*/
struct Stage
{
    s32 pixl;
    s32 chapter;
    s32 cap;  // highest story value at which the join can still happen
};
static const Stage sStages[] = {
    {PIXL_BOOMER, 2, 72},    // < 72: chest-appears scene; 72: chest holding Boomer; 73+: opened chest
    {PIXL_THUDLEY, 3, 107},  // < 108: Thudley's scene
    {PIXL_FLEEP, 4, 141},    // < 141: first knock; 141-144: give the paper, Fleep joins; 145+: empty
};

enum PatchKind
{
    GATE,       // IF_SMALL(GSW(0), a) -> IF_EQUAL(GSWF(join flag of pixl b), 0)
    NEVER,      // IF_SMALL(GSW(0), a) -> IF_SMALL(GSW(0), 0): the block never runs
    GIFT,       // USER_FUNC(evt_pouch_add_item, a) -> USER_FUNC(ap_join_gift, a)
    SAFETY_NET, // IF_EQUAL(LW(0), 0) -> never true
    STAGE,      // SWITCH(GSW(0)) -> SWITCH(GSW(AP_GSW_JOIN_STAGES + a))
    CASE_NEVER, // CASE_SMALL(a) -> CASE_SMALL(b), where b makes it unreachable
    HAVE_NEVER, // IF_NOT_EQUAL(LW(0), 0) -> IF_NOT_EQUAL(LW(0), LW(0)): never true
};

struct Patch
{
    PatchKind kind;
    u32 offset;  // from relF_section5 (relF.rel file offset - 0xAF300)
    s32 a, b;
    const char * what;
};

// Each was checked against the original instruction words in relF (see DESIGN.md)
static const Patch sPatches[] = {
    {GATE, 0x45490, 25, PIXL_THOREAU, "Thoreau: him and his chest"},
    {NEVER, 0x4556c, 25, 0, "Thoreau: exit locked (trap)"},
    {NEVER, 0x45600, 25, 0, "Thoreau: iron bars (trap)"},
    {GIFT, 0x44e5c, PIXL_THOREAU, 0, "Thoreau join"},
    {SAFETY_NET, 0x4149c, 0, 0, "Thoreau safety net"},

    {GATE, 0x54d48, 73, PIXL_BOOMER, "Boomer: him"},
    {STAGE, 0x54de0, 0, 0, "Boomer: chest stage"},
    {GIFT, 0x545a8, PIXL_BOOMER, 0, "Boomer join"},
    {SAFETY_NET, 0x52e24, 0, 0, "Boomer safety net"},

    {HAVE_NEVER, 0x6081c, 0, 0, "Slim: having Slim marks his chest opened"},
    {GIFT, 0x5f634, PIXL_SLIM, 0, "Slim join"},
    {SAFETY_NET, 0x650cc, 0, 0, "Slim safety net"},
    // Merlee's Mansion turns the Return Pipe off ("can't use it now") until Chapter 2 is 89, which
    // needs Slim; without him the pipe is the only way out (trap)
    {NEVER, 0x60df4, 89, 0, "Slim: mi3_01 Return Pipe off (trap)"},
    {NEVER, 0x61924, 89, 0, "Slim: mi3_02 Return Pipe off (trap)"},
    {NEVER, 0x62f10, 89, 0, "Slim: mi3_03 Return Pipe off (trap)"},
    {NEVER, 0x641f0, 89, 0, "Slim: mi3_04 Return Pipe off (trap)"},
    {NEVER, 0x65068, 89, 0, "Slim: mi3_05 Return Pipe off (trap)"},
    {NEVER, 0x66020, 89, 0, "Slim: mi3_06 Return Pipe off (trap)"},

    {GATE, 0x7f4f0, 108, PIXL_THUDLEY, "Thudley: him"},
    {STAGE, 0x7f588, 1, 0, "Thudley: scene stage"},
    {GIFT, 0x7ef14, PIXL_THUDLEY, 0, "Thudley join"},

    {GATE, 0x8f8a0, 123, PIXL_CARRIE, "Carrie: her and the cage"},
    {GIFT, 0x8f684, PIXL_CARRIE, 0, "Carrie join"},
    {SAFETY_NET, 0x91278, 0, 0, "Carrie safety net"},

    {STAGE, 0x9b328, 2, 0, "Fleep: outhouse stage"},
    {GIFT, 0x9beec, PIXL_FLEEP, 0, "Fleep join"},

    {GATE, 0xb9ee4, 192, PIXL_CUDGE, "Cudge: him"},
    {GATE, 0xb9fe8, 192, PIXL_CUDGE, "Cudge: tablet slots"},
    {CASE_NEVER, 0xb9f8c, 193, 189, "Cudge: exit pipe off at 189-192 (trap)"},
    {GIFT, 0xb9218, PIXL_CUDGE, 0, "Cudge join"},

    {GATE, 0xc7050, 209, PIXL_DOTTIE, "Dottie: her"},
    {GIFT, 0xc6e0c, PIXL_DOTTIE, 0, "Dottie join"},
    {SAFETY_NET, 0xcdffc, 0, 0, "Dottie safety net"},
};

// ap_join_gift(pixlItemId): a Pixl join location. Marks it collected; until Archipelago is
// connected it still gives the vanilla Pixl.
static EVT_DEFINE_USER_FUNC(ap_join_gift)
{
    (void) isFirstCall;
    s32 item = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    spm::swdrv::swSet(AP_GSWF_JOIN(item));
    // Debug: act like a join holding another world's item, to test the rooms without the Pixl
    if (!(mailbox::gMailbox.debugFlags & DEBUG_JOINS_GIVE_NOTHING))
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

static void apply(const Patch & p)
{
    EvtScriptCode * at = gamesyms::relF_section5 + p.offset / sizeof(EvtScriptCode);
    switch (p.kind)
    {
        case GATE:
        {
            const s32 orig[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), p.a};
            if (expect(at, orig, 3, p.what))
            {
                at[0] = EVT_CMD(2, OP_IF_EQUAL);
                at[1] = GSWF_VAR(AP_GSWF_JOIN(p.b));
                at[2] = 0;
            }
            break;
        }
        case NEVER:
        {
            const s32 orig[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), p.a};
            if (expect(at, orig, 3, p.what))
                at[2] = 0;
            break;
        }
        case GIFT:
        {
            const s32 orig[] = {EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_pouch::evt_pouch_add_item), p.a};
            if (expect(at, orig, 3, p.what))
                at[1] = reinterpret_cast<s32>(ap_join_gift);
            break;
        }
        case SAFETY_NET:
        {
            const s32 orig[] = {EVT_CMD(2, OP_IF_EQUAL), LW_VAR(0), 0};
            if (expect(at, orig, 3, p.what))
                at[2] = -1;
            break;
        }
        case STAGE:
        {
            const s32 orig[] = {EVT_CMD(1, OP_SWITCH), GSW_VAR(0)};
            if (expect(at, orig, 2, p.what))
                at[1] = GSW_VAR(AP_GSW_JOIN_STAGES + p.a);
            break;
        }
        case CASE_NEVER:
        {
            const s32 orig[] = {EVT_CMD(1, OP_CASE_SMALL), p.a};
            if (expect(at, orig, 2, p.what))
                at[1] = p.b;
            break;
        }
        case HAVE_NEVER:
        {
            const s32 orig[] = {EVT_CMD(2, OP_IF_NOT_EQUAL), LW_VAR(0), 0};
            if (expect(at, orig, 3, p.what))
                at[2] = LW_VAR(0);
            break;
        }
    }
}

void init()
{
    for (auto & p : sPatches)
        apply(p);
    wii::os::OSReport("[spmap] joins: patched\n");
}

void onMapChange()
{
    for (u32 i = 0; i < sizeof(sStages) / sizeof(sStages[0]); i++)
    {
        const Stage & s = sStages[i];
        s32 value = STAGE_DONE;
        if (!spm::swdrv::swGet(AP_GSWF_JOIN(s.pixl)))
        {
            value = storyvalue::getChapterValue(s.chapter);
            if (value > s.cap)
                value = s.cap;
        }
        savedata::setGswByte(AP_GSW_JOIN_STAGES + i, static_cast<u8>(value));
    }
}

}
