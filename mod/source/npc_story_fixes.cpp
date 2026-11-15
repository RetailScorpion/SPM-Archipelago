#include <common.h>
#include <evt_cmd.h>
#include <spm/evtmgr_cmd.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "game_mode.h"
#include "mailbox.h"
#include "npc_story_fixes.h"
#include "pixl_joins.h"
#include "save_data.h"
#include "script_patching.h"
#include "story_gifts.h"
#include "story_progress.h"

namespace mod::npc_story_fixes {

using namespace mod::script_patching;

#define PIXL_THOREAU 221

// ---- Watchitt ----

// His "go get help" case now reads save byte AP_GSW_WATCHITT, which tick sets:
#define WATCHITT_WAIT 26  // past 25: "go get help"
#define WATCHITT_SEND 0   // below every Chapter 1 value: he sends for the builder

// CASE_SMALL(25) in his script (relF offset) -> compares with AP_GSW_WATCHITT
#define WATCHITT_CASE 0x40f1c

void tick()
{
    save_data::setGswByte(AP_GSW_WATCHITT, pixl_joins::havePixl(PIXL_THOREAU) ? WATCHITT_SEND : WATCHITT_WAIT);
}

// ---- Bestovius ----

// His talk script now compares with save byte AP_GSW_LESSON instead of Chapter 1's value:
// 0 = offer the lesson; LESSON_DONE (past the lesson, below his later chats) = only chat
#define LESSON_DONE 17

// The IFs in his talk script that compare Chapter 1's value with a number (relF offset, the
// number, and which IF it is). Each now compares AP_GSW_LESSON instead.
static const struct
{
    u32 offset;
    s32 value;
    s32 opcode;
    const char * what;
} sLessonChecks[] = {
    {0x1645f8, 421, OP_IF_LARGE_EQUAL, "Bestovius: chat 421"},
    {0x164628, 354, OP_IF_LARGE_EQUAL, "Bestovius: chat 354"},
    {0x164658, 287, OP_IF_LARGE_EQUAL, "Bestovius: chat 287"},
    {0x164688, 221, OP_IF_LARGE_EQUAL, "Bestovius: chat 221"},
    {0x1646b8, 174, OP_IF_LARGE_EQUAL, "Bestovius: chat 174"},
    {0x1646e8, 126, OP_IF_LARGE_EQUAL, "Bestovius: chat 126"},
    {0x164718, 96, OP_IF_LARGE_EQUAL, "Bestovius: chat 96"},
    {0x164748, 17, OP_IF_LARGE_EQUAL, "Bestovius: chat 17"},
    {0x164778, 16, OP_IF_LARGE_EQUAL, "Bestovius: chat 16"},
    {0x1647a8, 15, OP_IF_SMALL, "Bestovius: first offer"},
};

// The end of his lesson, where vanilla sets Chapter 1's value to 16: SET(story value, 16) at this
// relF offset becomes a call to ap_lesson(16)
#define LESSON_END 0x164df0
#define LESSON_END_VALUE 16

// ap_lesson(value): the lesson is over. Collects its location, marks it done for his talk script,
// and moves Chapter 1 on to value as in vanilla. (Returns 2: finished.)
static EVT_DEFINE_USER_FUNC(ap_lesson)
{
    (void) isFirstCall;
    s32 value = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    story_gifts::collectGift(story_gifts::GIFT_BESTOVIUS);
    save_data::setGswByte(AP_GSW_LESSON, LESSON_DONE);
    story_progress::advanceChapter(1, value);
    return 2;
}

// ---- 7-3's big cloud (an3_15) ----

/*
    an3_15's setup puts the big bouncy cloud there by Chapter 7's story value: below 331 (before the
    Peach reunion) nothing, below 333 the cloud character who wants a Red Apple (its talk scene makes
    the bouncy cloud and sets 333), from 333 the bouncy cloud. With big_cloud_requires_peach_reunion
    off (mailbox SEED_FLAG_BIG_CLOUD_EARLY; user, 2026-11-05), both CASE_SMALLs compare with 0 instead,
    which the story value never is below: the bouncy cloud is there from the start, and the cloud
    character stays off screen, where the setup first puts it.
*/
#define CLOUD_CASE_REUNION 0x1094dc     // CASE_SMALL(331) (relF file 0x1b87dc)
#define CLOUD_CASE_APPLE 0x1094e4       // CASE_SMALL(333)
#define CLOUD_STORY_REUNION 331
#define CLOUD_STORY_APPLE 333
#define CASE_NEVER 0

static EvtScriptCode * sCloudCases[2];
static const s32 sCloudVanilla[2] = {CLOUD_STORY_REUNION, CLOUD_STORY_APPLE};

static void findCloudCases()
{
    const u32 offsets[2] = {CLOUD_CASE_REUNION, CLOUD_CASE_APPLE};
    for (s32 i = 0; i < 2; i++)
    {
        EvtScriptCode * at = relfScript(offsets[i]);
        const s32 expected[] = {EVT_CMD(1, OP_CASE_SMALL), sCloudVanilla[i]};
        if (!matches(at, expected, 2, "7-3 big cloud"))
        {
            sCloudCases[0] = sCloudCases[1] = nullptr;
            return;
        }
        sCloudCases[i] = at;
    }
}

// The cloud's two story checks, vanilla or never true, following the seed's setting.
static void setCloudCases()
{
    if (sCloudCases[0] == nullptr)
        return;
    bool early = mailbox::gMailbox.seedFlags & SEED_FLAG_BIG_CLOUD_EARLY;
    for (s32 i = 0; i < 2; i++)
        game_mode::set<s32>(&sCloudCases[i][1], early ? CASE_NEVER : sCloudVanilla[i]);
}

// ---- Locks that move the story past scenes still to come ----

/*
    Some locks set their chapter's story value when opened, past scenes that give a location's item.
    With the keys shuffled, the key can come first, and opening the lock then skipped those scenes for
    good. Each such lock now only sets a flag of its own when opened, and appears while that's clear; the
    story moves on, on a map change, once both are done (the lock opened, the scenes' value reached), as
    vanilla's order would have it. A save already past the lock's value gets the flag.
      - 7-2, an2_03's lock2 (Door Key 37): opening it set 316, past Bowser's reunion (an2_04: 313-314)
        and the D-Men (at exactly 314, their key at 315) (user, 2026-11-08).
      - 5-3, gn3_03's lock (Cave Key 33): opening it set 201, past Flint Cragley's scenes in gn3_01
        (below 200; his Cave Key is given before he sets 200) (user, 2026-11-10).
*/
static const struct
{
    u32 openSet;      // the lock's open script: SET(GSW(0), lockValue) (relF section 5)
    u32 appearsTest;  // the map's init: IF_SMALL(GSW(0), lockValue) around the lock
    s32 lockValue;
    s32 scenesDone;   // the value once the skipped scenes are over
    s32 chapter;
    s32 flag;
    const char * what;
} sStoryLocks[] = {
    {0xf18e0, 0xf2d3c, 316, 315, 7, AP_GSWF_UNDERWHERE_LOCK, "7-2 lock"},  // relF file 0x1a0be0, 0x1a203c
    {0xc0270, 0xc0b10, 201, 200, 5, AP_GSWF_CRAG_LOCK, "5-3 lock"},        // relF file 0x16f570, 0x16fe10
};

static void patchStoryLocks()
{
    for (auto & lock : sStoryLocks)
    {
        EvtScriptCode * set = relfScript(lock.openSet);
        const s32 setWords[] = {EVT_CMD(2, OP_SET), GSW_VAR(0), lock.lockValue};
        EvtScriptCode * test = relfScript(lock.appearsTest);
        const s32 testWords[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), lock.lockValue};
        if (!matches(set, setWords, 3, lock.what) || !matches(test, testWords, 3, lock.what))
            continue;
        game_mode::set<s32>(&set[1], GSWF_VAR(lock.flag));
        game_mode::set<s32>(&set[2], 1);
        game_mode::set<s32>(&test[0], EVT_CMD(2, OP_IF_EQUAL));
        game_mode::set<s32>(&test[1], GSWF_VAR(lock.flag));
        game_mode::set<s32>(&test[2], 0);
    }
}

static void syncStoryLocks()
{
    for (auto & lock : sStoryLocks)
    {
        s32 value = story_progress::getChapterValue(lock.chapter);
        if (value >= lock.lockValue)
            spm::swdrv::swSet(lock.flag);
        else if (value >= lock.scenesDone && spm::swdrv::swGet(lock.flag))
            story_progress::advanceChapter(lock.chapter, lock.lockValue);
    }
}

// ---- 7-1's lock to Luigi ----

/*
    7-1's lock in an1_03 (Door Key 49, on the door toward Luigi's rooms) is only there below story value
    296: in vanilla it belongs to the Underwhere visit after Chapter 6, and Chapter 7 starts at 303, so the
    randomizer never had it (2026-11-12). Its key's spot (Queen Jaydes, an1_04) is reachable again
    (check_stages), so the lock is back too (user, 2026-11-12): it appears while AP_GSWF_LUIGI_LOCK is
    clear, and opening it sets that flag instead of the story value. A save that already has Luigi's join
    collected counts it as open.
*/
#define LUIGI_LOCK_TEST 0xe78ec  // an1_03's init: IF_SMALL(GSW(0), 296) around the lock (relF 0x196bec)
#define LUIGI_LOCK_OPEN 0xe74d8  // its open script: SET(GSW(0), 296) (relF 0x1967d8)
#define LUIGI_LOCK_VALUE 296

static void patchLuigiLock()
{
    EvtScriptCode * test = relfScript(LUIGI_LOCK_TEST);
    const s32 testWords[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), LUIGI_LOCK_VALUE};
    EvtScriptCode * set = relfScript(LUIGI_LOCK_OPEN);
    const s32 setWords[] = {EVT_CMD(2, OP_SET), GSW_VAR(0), LUIGI_LOCK_VALUE};
    if (!matches(test, testWords, 3, "7-1 lock") || !matches(set, setWords, 3, "7-1 lock"))
        return;
    game_mode::set<s32>(&test[0], EVT_CMD(2, OP_IF_EQUAL));
    game_mode::set<s32>(&test[1], GSWF_VAR(AP_GSWF_LUIGI_LOCK));
    game_mode::set<s32>(&test[2], 0);
    game_mode::set<s32>(&set[1], GSWF_VAR(AP_GSWF_LUIGI_LOCK));
    game_mode::set<s32>(&set[2], 1);
}

void onMapChange()
{
    setCloudCases();
    syncStoryLocks();
    if (!spm::swdrv::swGet(AP_GSWF_LUIGI_LOCK) && spm::swdrv::swGet(AP_GSWF_GIFT(story_gifts::GIFT_LUIGI)))
        spm::swdrv::swSet(AP_GSWF_LUIGI_LOCK);
    bool lessonCollected = spm::swdrv::swGet(AP_GSWF_GIFT(story_gifts::GIFT_BESTOVIUS));
    save_data::setGswByte(AP_GSW_LESSON, lessonCollected ? LESSON_DONE : 0);
}

// ---- Patching ----

// Makes one of Bestovius's IFs compare AP_GSW_LESSON instead of the story value.
static void patchLessonCheck(u32 offset, s32 value, s32 opcode, const char * what)
{
    EvtScriptCode * at = relfScript(offset);
    const s32 expected[] = {EVT_CMD(2, opcode), GSW_VAR(0), value};
    if (matches(at, expected, 3, what))
        game_mode::set<s32>(&at[1], GSW_VAR(AP_GSW_LESSON));
}

// Turns the lesson's SET(story value, 16) into ap_lesson(16).
static void patchLessonEnd()
{
    EvtScriptCode * at = relfScript(LESSON_END);
    const s32 expected[] = {EVT_CMD(2, OP_SET), GSW_VAR(0), LESSON_END_VALUE};
    if (matches(at, expected, 3, "Bestovius: lesson"))
    {
        game_mode::set<s32>(&at[0], EVT_CMD(2, OP_USER_FUNC));
        game_mode::set<s32>(&at[1], reinterpret_cast<s32>(ap_lesson));
    }
}

void init()
{
    findCloudCases();
    caseComparesWithSaveByte(relfScript(WATCHITT_CASE), 25, AP_GSW_WATCHITT, "Watchitt: waits for Thoreau");
    for (auto & check : sLessonChecks)
        patchLessonCheck(check.offset, check.value, check.opcode, check.what);
    patchLessonEnd();
    patchStoryLocks();
    patchLuigiLock();
    wii::os::OSReport("[spmap] joins: Watchitt and Bestovius patched\n");
}

}
