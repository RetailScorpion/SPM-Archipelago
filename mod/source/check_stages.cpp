#include <common.h>
#include <evt_cmd.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "check_stages.h"
#include "game_mode.h"
#include "save_data.h"
#include "script_patching.h"
#include "story_gifts.h"
#include "story_progress.h"

namespace mod::check_stages {

using namespace mod::script_patching;

// The first evt global word used for the stages; GW 16-31 appear in no game script
#define FIRST_STAGE_GW 16
#define GW_WORD(id) (-50000000 + (id))

// No saved step (sequences only)
#define NO_STEP 0

/*
    The checks. `last` is the highest story value at which the check is still there in vanilla;
    `restart` the stage it goes back to once the story has passed it uncollected (its scene's start).
    `stepByte` is the save byte (AP_GSW_CHECK_STEPS + n) of a check whose steps span several visits,
    or -1. `after` is the check whose scene has to come first (its index), or -1: until that one is
    collected, the stage stays just below `restart` (user, 2026-11-12: the D-Men gave their key on
    entering an2_03 with the story past them, before Bowser's fight).
*/
static const struct
{
    s32 chapter;
    s32 flag;
    s32 last;
    s32 restart;
    s32 stepByte;
    s32 after;
    const char * what;
} sChecks[] = {
    {1, AP_GSWF_LOC_BASE + 16, 42, 42, -1, -1, "1-4 Ruins Key (he4_05)"},
    {1, AP_GSWF_LOC_BASE + 17, 45, 45, -1, -1, "1-4 Ruins Key (he4_07)"},
    {2, AP_GSWF_LOC_BASE + 19, 78, 78, -1, -1, "2-2 House Key (mi2_06)"},
    {4, AP_GSWF_LOC_BASE + 21, 145, 145, -1, -1, "4-2 Door Key (sp2_01)"},
    {5, AP_GSWF_LOC_BASE + 25, 199, 199, -1, -1, "5-3 Cave Key (gn3_01, Flint Cragley)"},
    {5, AP_GSWF_LOC_BASE + 27, 210, 210, -1, -1, "5-4 Card Key (gn4_10)"},
    {7, AP_GSWF_GIFT(story_gifts::GIFT_BOWSER_REUNION), 313, 313, -1, -1, "7-2 Bowser rejoins (an2_04)"},
    {7, AP_GSWF_LOC_BASE + 8, 314, 314, -1, 6, "7-2 Door Key (an2_03, the D-Men)"},
    {7, AP_GSWF_LOC_BASE + 9, 321, 319, 0, -1, "7-2 Door Key (an2_07, the sisters)"},
    {7, AP_GSWF_LOC_BASE + 11, 343, 341, 1, -1, "7-4 Door Key (an4_04, the angel)"},
    {7, AP_GSWF_LOC_BASE + 10, 347, 347, -1, -1, "7-4 Door Key (an4_03)"},
    {8, AP_GSWF_LOC_BASE + 29, 374, 374, -1, -1, "8-2 Castle Bleck Key (ls2_02)"},
    {8, AP_GSWF_LOC_BASE + 30, 376, 376, -1, -1, "8-2 Castle Bleck Key (ls2_12)"},
    // (user, 2026-11-12: missed by the scan, which looked for tests around spawns; this one swaps
    // Jaydes's talk script, and Chapter 7 starts at 303)
    {7, AP_GSWF_LOC_BASE + 7, 294, 294, -1, -1, "7-1 Door Key (an1_04, Queen Jaydes)"},
    // (tools/talk_swaps.py, 2026-11-12: once Chapter 7 is finished, the orb angels' talk becomes their
    // post-game lines; with the orbs shuffled, the chapter can be finished without their spots)
    {7, AP_GSWF_LOC_BASE + 12, 353, 353, -1, -1, "7-4 Blue Orb (an4_04)"},
    {7, AP_GSWF_LOC_BASE + 14, 353, 353, -1, -1, "7-4 Red Orb (an4_07)"},
};
#define CHECK_COUNT (sizeof(sChecks) / sizeof(sChecks[0]))
static_assert(FIRST_STAGE_GW + CHECK_COUNT <= 32, "out of evt global words (GW 16-31) for check stages");

enum PatchKind
{
    TEST,   // IF_*(story value, n) or SWITCH(story value): reads the check's stage instead
    RAISE,  // SET(story value, n) -> ap_story_raise(n): only ever moves the story forward
    STEP,   // SET(story value, n) -> ap_check_step(check, n): also records the check's step
};

static const struct
{
    PatchKind kind;
    u32 offset;  // relF script section offset (relF.rel file offset - 0xAF300)
    s32 check;   // index in sChecks
    s32 value;   // RAISE/STEP: the value set
} sPatches[] = {
    // 1-4 he4_05: the key's spawn; its pickup sets 43 (relF file 0xfa394, 0xf9f38)
    {TEST, 0x4b094, 0, 0},
    {RAISE, 0x4ac38, 0, 43},
    // 1-4 he4_07: the key's spawn; its pickup sets 46 (0xfac4c, 0xfab78)
    {TEST, 0x4b94c, 1, 0},
    {RAISE, 0x4b878, 1, 46},
    // 2-2 mi2_06: the key's box; its pickup sets 79 (0x10abbc, 0x10aa30)
    {TEST, 0x5b8bc, 2, 0},
    {RAISE, 0x5b730, 2, 79},
    // 4-2 sp2_01: the hidden spot Fleep reveals; its pickup sets 146 (0x14cda0, 0x14b6b8)
    {TEST, 0x9daa0, 3, 0},
    {RAISE, 0x9c3b8, 3, 146},
    // 5-3 gn3_01: Flint Cragley's spawn, his key scene (once both troops are found) and his talk;
    // his scenes set 199 and 200 (0x16e518, 0x16ec50, 0x16c37c, 0x16ccdc, 0x16e1ac)
    {TEST, 0xbf218, 4, 0},
    {TEST, 0xbf950, 4, 0},
    {TEST, 0xbd07c, 4, 0},
    {RAISE, 0xbd9dc, 4, 199},
    {RAISE, 0xbeeac, 4, 200},
    // 5-4 gn4_10: the Card Key's spawn; its pickup sets 211 (0x177124, 0x176e5c)
    {TEST, 0xc7e24, 5, 0},
    {RAISE, 0xc7b5c, 5, 211},
    // 7-2 an2_04: Bowser and his fight, and the room's quakes; the scene sets 313 and 314
    // (0x1a38f8, 0x1a3a5c, 0x1a3ad4, 0x1a36c0, 0x1a30d8)
    {TEST, 0xf45f8, 6, 0},
    {TEST, 0xf475c, 6, 0},
    {TEST, 0xf47d4, 6, 0},
    {RAISE, 0xf43c0, 6, 313},
    {RAISE, 0xf3dd8, 6, 314},
    // 7-2 an2_03: the D-Men's scene (story exactly 314, after Bowser's), where they stand, and the angel;
    // the scene sets 315 (0x1a266c, 0x1a23e0, 0x1a2200, 0x1a220c, 0x1a24d8, 0x1a1aa0)
    {TEST, 0xf336c, 7, 0},
    {TEST, 0xf30e0, 7, 0},
    {TEST, 0xf2f00, 7, 0},
    {TEST, 0xf2f0c, 7, 0},
    {TEST, 0xf31d8, 7, 0},
    {RAISE, 0xf27a0, 7, 315},
    // 7-2 an2_07 and an2_01: the sisters' talk and the an2_01 errand they send Mario on; 320 (taken
    // on), 321 (errand done), 322 (the key) (0x1a6444, 0x19ecb4, 0x1a6710, 0x19ed64, 0x1a6da0)
    {TEST, 0xf7144, 8, 0},
    {TEST, 0xef9b4, 8, 0},
    {STEP, 0xf7410, 8, 320},
    {STEP, 0xefa64, 8, 321},
    {STEP, 0xf7aa0, 8, 322},
    // 7-4 an4_04: the angel's scenes (intro, the fight, the key), the music, and the later angel that
    // would duplicate her; 342, 343, 344 (0x1be7c8, 0x1bf0e4, 0x1bed34, 0x1bddb8, 0x1bd954, 0x1bd7e4)
    {TEST, 0x10f4c8, 9, 0},
    {TEST, 0x10fde4, 9, 0},
    {TEST, 0x10fa34, 9, 0},
    {STEP, 0x10eab8, 9, 342},
    {STEP, 0x10e654, 9, 343},
    {STEP, 0x10e4e4, 9, 344},
    // 7-4 an4_03: the frozen character who gives the key; sets 348 (0x1bd074, 0x1bc3f8)
    {TEST, 0x10dd74, 10, 0},
    {RAISE, 0x10d0f8, 10, 348},
    // 8-2 ls2_02: the key's box; its pickup sets 375 (0x1d6bb4, 0x1d60c8)
    {TEST, 0x1278b4, 11, 0},
    {RAISE, 0x126dc8, 11, 375},
    // 8-2 ls2_12: the key after the fight; its pickup sets 377 (0x1dbcd4, 0x1dbc80)
    {TEST, 0x12c9d4, 12, 0},
    {RAISE, 0x12c980, 12, 377},
    // 7-1 an1_04: Queen Jaydes's talk is the key scene below 295 (an IF and a SWITCH that swap it);
    // the scene sets 295 (0x19ae20, 0x19b0b0, 0x199b04)
    {TEST, 0xebb20, 13, 0},
    {TEST, 0xebdb0, 13, 0},
    {RAISE, 0xea804, 13, 295},
    // 7-4 an4_04 and an4_07: the orb angel's post-game version (story 354+) replaces the one with the
    // orb (0x1bf008, 0x1c25dc)
    {TEST, 0x10fd08, 14, 0},
    {TEST, 0x1132dc, 15, 0},
};

// ap_story_raise(value): the story value moves to value, unless it's already past it.
static EVT_DEFINE_USER_FUNC(ap_story_raise)
{
    (void) isFirstCall;
    s32 value = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    s32 & story = spm::spmario::gp->gsw0;
    if (story < value)
        story = value;
    return 2;
}

#define STEP_PACK 10000  // ap_check_step's argument: check * STEP_PACK + value

static void setStage(u32 check, s32 value)
{
    spm::evtmgr::evtGetWork()->gw[FIRST_STAGE_GW + check] = value;
}

// ap_check_step(check * 10000 + value): the check's scene reached step value; the story moves there too
// unless it's already past it.
static EVT_DEFINE_USER_FUNC(ap_check_step)
{
    (void) isFirstCall;
    s32 packed = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    u32 check = static_cast<u32>(packed / STEP_PACK);
    s32 value = packed % STEP_PACK;
    if (check < CHECK_COUNT && sChecks[check].stepByte >= 0)
    {
        save_data::setGswByte(AP_GSW_CHECK_STEPS + sChecks[check].stepByte,
                              static_cast<u8>(value - sChecks[check].restart + 1));
        setStage(check, value);
    }
    s32 & story = spm::spmario::gp->gsw0;
    if (story < value)
        story = value;
    return 2;
}

void onMapChange()
{
    for (u32 i = 0; i < CHECK_COUNT; i++)
    {
        auto & check = sChecks[i];
        s32 value = story_progress::getChapterValue(check.chapter);
        s32 stage;
        u8 step = check.stepByte >= 0 ? save_data::getGswByte(AP_GSW_CHECK_STEPS + check.stepByte) : NO_STEP;
        if (spm::swdrv::swGet(check.flag))
            stage = value > check.last ? value : check.last + 1;
        else if (step != NO_STEP)
            stage = check.restart + step - 1;
        else if (value <= check.last)
            stage = value;
        else
            stage = check.restart;
        // a check whose scene follows another's waits for that one (its stage just short of its scene)
        if (check.after >= 0 && !spm::swdrv::swGet(sChecks[check.after].flag) && stage >= check.restart)
            stage = check.restart - 1;
        setStage(i, stage);
    }
}

void init()
{
    for (auto & patch : sPatches)
    {
        EvtScriptCode * at = relfScript(patch.offset);
        const char * what = sChecks[patch.check].what;
        if (patch.kind == TEST)
        {
            const s32 story[] = {GSW_VAR(0)};
            if (matches(at + 1, story, 1, what))
                game_mode::set<s32>(&at[1], GW_WORD(FIRST_STAGE_GW + patch.check));
            continue;
        }
        const s32 expected[] = {EVT_CMD(2, OP_SET), GSW_VAR(0), patch.value};
        if (!matches(at, expected, 3, what))
            continue;
        game_mode::set<s32>(&at[0], EVT_CMD(2, OP_USER_FUNC));
        if (patch.kind == RAISE)
        {
            game_mode::set<s32>(&at[1], reinterpret_cast<s32>(ap_story_raise));
        }
        else
        {
            game_mode::set<s32>(&at[1], reinterpret_cast<s32>(ap_check_step));
            game_mode::set<s32>(&at[2], patch.check * STEP_PACK + patch.value);
        }
    }
    wii::os::OSReport("[spmap] check stages: %d checks follow their own stage\n", static_cast<s32>(CHECK_COUNT));
}

}
