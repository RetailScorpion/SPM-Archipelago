#include <common.h>
#include <evt_cmd.h>
#include <spm/evtmgr_cmd.h>
#include <spm/swdrv.h>
#include <wii/os.h>

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

void onMapChange()
{
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
        at[1] = GSW_VAR(AP_GSW_LESSON);
}

// Turns the lesson's SET(story value, 16) into ap_lesson(16).
static void patchLessonEnd()
{
    EvtScriptCode * at = relfScript(LESSON_END);
    const s32 expected[] = {EVT_CMD(2, OP_SET), GSW_VAR(0), LESSON_END_VALUE};
    if (matches(at, expected, 3, "Bestovius: lesson"))
    {
        at[0] = EVT_CMD(2, OP_USER_FUNC);
        at[1] = reinterpret_cast<s32>(ap_lesson);
    }
}

void init()
{
    caseComparesWithSaveByte(relfScript(WATCHITT_CASE), 25, AP_GSW_WATCHITT, "Watchitt: waits for Thoreau");
    for (auto & check : sLessonChecks)
        patchLessonCheck(check.offset, check.value, check.opcode, check.what);
    patchLessonEnd();
    wii::os::OSReport("[spmap] joins: Watchitt and Bestovius patched\n");
}

}
