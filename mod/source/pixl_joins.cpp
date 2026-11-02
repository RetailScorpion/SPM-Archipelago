#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_pouch.h>
#include <spm/evtmgr_cmd.h>
#include <spm/item_data_ids.h>
#include <spm/mario_pouch.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "archipelago_item.h"
#include "code_patching.h"
#include "game_addresses.h"
#include "mailbox.h"
#include "pixl_joins.h"
#include "received_items.h"
#include "save_data.h"
#include "script_patching.h"
#include "story_progress.h"

namespace mod::pixl_joins {

using namespace mod::script_patching;

// Pixls by item id
#define PIXL_THOREAU 221
#define PIXL_BOOMER 222
#define PIXL_SLIM 223
#define PIXL_THUDLEY 224
#define PIXL_CARRIE 225
#define PIXL_FLEEP 226
#define PIXL_CUDGE 227
#define PIXL_DOTTIE 228
#define PIXL_PICCOLO 229
#define PIXL_BARRY 230
#define PIXL_DASHELL 231

// Developer feature: location numbers the joins report to the mailbox (0x20000 + Pixl item id,
// plus 0x1000 when it held another world's item). Only the developer tools read them
#define JOIN_LOG_LOCATION_BASE 0x20000
#define JOIN_LOG_OTHER_WORLD 0x1000

// What a join gives with the DEBUG_JOINS_GIVE_PLACEHOLDER test setting, if no item was chosen
#define DEFAULT_PLACEHOLDER_ITEM 71  // Gold Bar

// ---- Scene steps ----

/*
    Joins whose scene step comes from a SWITCH on the story value. The patched SWITCH reads save
    byte AP_GSW_JOIN_STAGES + n (n = position in this list), which onMapChange sets to the chapter's
    value (no higher than `lastStep`) until the join is collected, then to STAGE_DONE.
*/
struct Stage
{
    s32 pixl;
    s32 chapter;
    s32 lastStep;  // the highest story value at which the join can still happen
};
static const Stage sStages[] = {
    {PIXL_BOOMER, 2, 72},    // below 72: the chest appears; 72: the chest holds Boomer; 73+: opened
    {PIXL_THUDLEY, 3, 107},  // below 108: Thudley's scene
    {PIXL_FLEEP, 4, 141},    // below 141: first knock; 141-144: give the paper, Fleep joins; 145+: empty
};

// The step value meaning "join done", past every vanilla step
#define STAGE_DONE 200

// ---- The patches ----

enum PatchKind
{
    GATE,        // IF_SMALL(story value, a) -> IF_EQUAL(join flag of Pixl b, 0): runs until collected
    NEVER,       // IF_SMALL(story value, a) -> IF_SMALL(story value, 0): the block never runs
    GIFT,        // the "add Pixl a" and "select Pixl a" calls -> ap_join_gift and ap_join_select
    SAFETY_NET,  // IF_EQUAL(LW(0), 0) -> IF_EQUAL(LW(0), -1): the safety net never gives the Pixl
    STAGE,       // SWITCH(story value) -> SWITCH(save byte AP_GSW_JOIN_STAGES + a)
    CASE_NEVER,  // CASE_SMALL(a) -> CASE_SMALL(b), a value the story never has there
    HAVE_NEVER,  // IF_NOT_EQUAL(LW(0), 0) -> IF_NOT_EQUAL(LW(0), LW(0)): never true
    SKIP,        // a call with a arguments to function b (SkippableCall) -> does nothing
    FLEEP_ONLY,  // a call to Fleep-only function b -> only runs if Mario has Fleep
};

struct Patch
{
    PatchKind kind;
    u32 offset;  // bytes from the start of relF's script section (relF.rel offset - 0xAF300)
    s32 a, b;
    const char * what;
};

// Each one was checked against the game's original words (see DESIGN.md)
static const Patch sPatches[] = {
    // Thoreau (1-1)
    {GATE, 0x45490, 25, PIXL_THOREAU, "Thoreau: him and his chest"},
    {NEVER, 0x4556c, 25, 0, "Thoreau: exit locked (trap)"},
    {NEVER, 0x45600, 25, 0, "Thoreau: iron bars (trap)"},
    {GIFT, 0x44e5c, PIXL_THOREAU, 0, "Thoreau join"},
    {SAFETY_NET, 0x4149c, 0, 0, "Thoreau safety net"},

    // Boomer (2-1)
    {GATE, 0x54d48, 73, PIXL_BOOMER, "Boomer: him"},
    {STAGE, 0x54de0, 0, 0, "Boomer: chest stage"},
    {GIFT, 0x545a8, PIXL_BOOMER, 0, "Boomer join"},
    {SAFETY_NET, 0x52e24, 0, 0, "Boomer safety net"},

    // Slim (2-3)
    {HAVE_NEVER, 0x6081c, 0, 0, "Slim: having Slim marks his chest opened"},
    {GIFT, 0x5f634, PIXL_SLIM, 0, "Slim join"},
    {SAFETY_NET, 0x650cc, 0, 0, "Slim safety net"},
    // Merlee's Mansion switches the Return Pipe off ("can't use it now") until Chapter 2 reaches 89,
    // which needs Slim; without him the pipe is the only way out
    {NEVER, 0x60df4, 89, 0, "Slim: mi3_01 Return Pipe off (trap)"},
    {NEVER, 0x61924, 89, 0, "Slim: mi3_02 Return Pipe off (trap)"},
    {NEVER, 0x62f10, 89, 0, "Slim: mi3_03 Return Pipe off (trap)"},
    {NEVER, 0x641f0, 89, 0, "Slim: mi3_04 Return Pipe off (trap)"},
    {NEVER, 0x65068, 89, 0, "Slim: mi3_05 Return Pipe off (trap)"},
    {NEVER, 0x66020, 89, 0, "Slim: mi3_06 Return Pipe off (trap)"},

    // Thudley (3-2)
    {GATE, 0x7f4f0, 108, PIXL_THUDLEY, "Thudley: him"},
    {STAGE, 0x7f588, 1, 0, "Thudley: scene stage"},
    {GIFT, 0x7ef14, PIXL_THUDLEY, 0, "Thudley join"},
    // ta2_04 is a dead end: bars close behind Mario, only ground-pounding the pile (needs Thudley)
    // opens them, and the Return Pipe is always off there. Now the bars never close or reopen; the
    // pile still sets the story value to 108, so getting further still needs Thudley
    {SKIP, 0x7f050, 2, CALL_DOOR_ENABLE_DISABLE, "Thudley: door barred (trap)"},
    {SKIP, 0x7f078, 4, CALL_SOUND_3D, "Thudley: bars closing sound"},
    {SKIP, 0x7f090, 3, CALL_MAP_ANIMATION, "Thudley: bars closing"},
    {SKIP, 0x7f360, 4, CALL_SOUND_3D, "Thudley: bars opening sound"},
    {SKIP, 0x7f378, 3, CALL_MAP_ANIMATION, "Thudley: bars opening"},

    // Carrie (3-3)
    {GATE, 0x8f8a0, 123, PIXL_CARRIE, "Carrie: her and the cage"},
    {GIFT, 0x8f684, PIXL_CARRIE, 0, "Carrie join"},
    {SAFETY_NET, 0x91278, 0, 0, "Carrie safety net"},

    // Fleep (4-2)
    {STAGE, 0x9b328, 2, 0, "Fleep: outhouse stage"},
    {GIFT, 0x9beec, PIXL_FLEEP, 0, "Fleep join"},
    {FLEEP_ONLY, 0x9c2e4, 0, 0, "Fleep: pointer mode on (crashes without him)"},
    {FLEEP_ONLY, 0x9c2f0, 0, 1, "Fleep: pointer mode set (crashes without him)"},

    // Cudge (5-2)
    {GATE, 0xb9ee4, 192, PIXL_CUDGE, "Cudge: him"},
    {GATE, 0xb9fe8, 192, PIXL_CUDGE, "Cudge: tablet slots"},
    {CASE_NEVER, 0xb9f8c, 193, 189, "Cudge: exit pipe off at 189-192 (trap)"},
    {GIFT, 0xb9218, PIXL_CUDGE, 0, "Cudge join"},

    // Dottie (5-4)
    {GATE, 0xc7050, 209, PIXL_DOTTIE, "Dottie: her"},
    {GIFT, 0xc6e0c, PIXL_DOTTIE, 0, "Dottie join"},
    {SAFETY_NET, 0xcdffc, 0, 0, "Dottie safety net"},

    // Optional Pixls: their scenes don't check the story value (tested), so only the gift is patched
    {GIFT, 0x33684, PIXL_PICCOLO, 0, "Piccolo join"},
    {GIFT, 0x7b024, PIXL_BARRY, 0, "Barry join"},
    {GIFT, 0x15b6d4, PIXL_DASHELL, 0, "Dashell join"},
};

// ---- Script functions the patched scenes call ----
// (Each returns 2, which tells the script engine the function has finished.)

// Whether Mario has this Pixl.
bool havePixl(s32 pixl)
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->pixls)
    {
        if (info.itemType == pixl)
            return true;
    }
    return false;
}

/*
    ap_join_gift(pixl): a Pixl join location. Marks it collected and gives what it holds. For
    testing, the mailbox can make a join give another item (per Pixl, joinItems) or nothing or a
    placeholder (every join, debugFlags); otherwise it gives the Pixl itself.
*/
static EVT_DEFINE_USER_FUNC(ap_join_gift)
{
    (void) isFirstCall;
    s32 pixl = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    spm::swdrv::swSet(AP_GSWF_JOIN(pixl));

    mailbox::Mailbox & mb = mailbox::gMailbox;
    u16 given = mb.joinItems[pixl - ITEM_ID_FAIRY_START];
    if (given == JOIN_ITEM_OTHER_WORLD)
    {
        received_items::giveLocal(archipelago_item::AP_ITEM_ID);
        wii::os::OSReport("[spmap] joins: Pixl %d's location holds another world's item\n", pixl);
    }
    else if (given != 0)
        received_items::giveLocal(given);
    else if (mb.debugFlags & DEBUG_JOINS_GIVE_NOTHING)
        ;  // test setting: give nothing
    else if (mb.debugFlags & DEBUG_JOINS_GIVE_PLACEHOLDER)
        received_items::giveLocal(mb.placeholderItem ? mb.placeholderItem : DEFAULT_PLACEHOLDER_ITEM);
    else
        spm::mario_pouch::pouchAddItem(pixl);

    mailbox::sendLocation(JOIN_LOG_LOCATION_BASE + pixl + (given == JOIN_ITEM_OTHER_WORLD ? JOIN_LOG_OTHER_WORLD : 0));
    wii::os::OSReport("[spmap] joins: location for Pixl %d collected\n", pixl);
    return 2;
}

// ap_join_select(pixl): selects the Pixl only if Mario has it. Selecting one he doesn't have leaves
// no Pixl selected, and opening the Pixl menu then crashes.
static EVT_DEFINE_USER_FUNC(ap_join_select)
{
    (void) isFirstCall;
    s32 pixl = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    if (havePixl(pixl))
        spm::mario_pouch::pouchSetPixlSelected(pixl);
    return 2;
}

// The two calls in Fleep's join scene that only work if Mario really has Fleep: run them only then.
static EVT_DEFINE_USER_FUNC(ap_fleep_mode_on)
{
    return havePixl(PIXL_FLEEP) ? game_addresses::evt_fleep_mode_on(evt, isFirstCall) : 2;
}
static EVT_DEFINE_USER_FUNC(ap_fleep_mode_set)
{
    return havePixl(PIXL_FLEEP) ? game_addresses::evt_fleep_mode_set(evt, isFirstCall) : 2;
}

// FLEEP_ONLY patches: the game's function (b = 0 or 1) and the mod's version
using ScriptFunction = s32(spm::evtmgr::EvtEntry * evt, bool isFirstCall);
static const struct
{
    ScriptFunction * game;
    ScriptFunction * fleepOnly;
} sFleepOnly[] = {
    {game_addresses::evt_fleep_mode_on, ap_fleep_mode_on},
    {game_addresses::evt_fleep_mode_set, ap_fleep_mode_set},
};

// ---- Applying the patches ----

// Applies one patch (after checking the words it changes are what's expected).
static void applyPatch(const Patch & p)
{
    EvtScriptCode * at = relfScript(p.offset);
    switch (p.kind)
    {
        case GATE:
        {
            const s32 expected[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), p.a};
            if (matches(at, expected, 3, p.what))
            {
                at[0] = EVT_CMD(2, OP_IF_EQUAL);
                at[1] = GSWF_VAR(AP_GSWF_JOIN(p.b));
                at[2] = 0;
            }
            break;
        }
        case NEVER:
        {
            const s32 expected[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), p.a};
            if (matches(at, expected, 3, p.what))
                at[2] = 0;
            break;
        }
        case GIFT:
        {
            const s32 expected[] = {
                EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_pouch::evt_pouch_add_item), p.a,
                EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_pouch::evt_pouch_set_pixl_selected), p.a,
            };
            if (matches(at, expected, 6, p.what))
            {
                at[1] = reinterpret_cast<s32>(ap_join_gift);
                at[4] = reinterpret_cast<s32>(ap_join_select);
            }
            break;
        }
        case SAFETY_NET:
        {
            const s32 expected[] = {EVT_CMD(2, OP_IF_EQUAL), LW_VAR(0), 0};
            if (matches(at, expected, 3, p.what))
                at[2] = -1;
            break;
        }
        case STAGE:
        {
            const s32 expected[] = {EVT_CMD(1, OP_SWITCH), GSW_VAR(0)};
            if (matches(at, expected, 2, p.what))
                at[1] = GSW_VAR(AP_GSW_JOIN_STAGES + p.a);
            break;
        }
        case CASE_NEVER:
        {
            const s32 expected[] = {EVT_CMD(1, OP_CASE_SMALL), p.a};
            if (matches(at, expected, 2, p.what))
                at[1] = p.b;
            break;
        }
        case HAVE_NEVER:
        {
            const s32 expected[] = {EVT_CMD(2, OP_IF_NOT_EQUAL), LW_VAR(0), 0};
            if (matches(at, expected, 3, p.what))
                at[2] = LW_VAR(0);
            break;
        }
        case SKIP:
            skipCall(at, p.a, static_cast<SkippableCall>(p.b), p.what);
            break;
        case FLEEP_ONLY:
        {
            const s32 expected[] = {EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(sFleepOnly[p.b].game), 1};
            if (matches(at, expected, 3, p.what))
                at[1] = reinterpret_cast<s32>(sFleepOnly[p.b].fleepOnly);
            break;
        }
    }
}

// ---- Flipping without a Pixl ----

/*
    When Mario flips, the game looks up his current Pixl (by the id it keeps in Mario's work) and sets
    a flag in it, without checking the lookup found one. With no Pixl in the party (possible once
    Pixls are shuffled), that's a write to address 0: harmless on Dolphin without MMU, but it stops
    the game with a warning (user, 2026-11-02). The lookup goes through here instead, which gives a
    spare spot for the flag when there's no Pixl.
*/
static u32 sNoPixl;

static void * flipPixlLookup(s32 id)
{
    void * pixl = game_addresses::fairy_work_by_id(id);
    return pixl != nullptr ? pixl : &sNoPixl;
}

void init()
{
    for (auto & p : sPatches)
        applyPatch(p);
    code_patching::writeBranch(game_addresses::flip_pixl_lookup_call, reinterpret_cast<void *>(flipPixlLookup), true);
    wii::os::OSReport("[spmap] joins: Pixl joins patched\n");
}

// ---- Every map change ----

void repairPixlSelection()
{
    s32 first = -1;
    for (auto & info : spm::mario_pouch::pouchGetPtr()->pixls)
    {
        if (info.selected)
            return;
        if (first == -1 && info.itemType >= PIXL_THOREAU && info.itemType < ITEM_ID_FAIRY_MAX)
            first = info.itemType;
    }
    if (first != -1)
    {
        spm::mario_pouch::pouchSetPixlSelected(first);
        wii::os::OSReport("[spmap] joins: no Pixl was selected, selected %d\n", first);
    }
}

// The step value a join's patched SWITCH should see now.
static s32 stageValue(const Stage & stage)
{
    if (spm::swdrv::swGet(AP_GSWF_JOIN(stage.pixl)))
        return STAGE_DONE;
    s32 value = story_progress::getChapterValue(stage.chapter);
    return value > stage.lastStep ? stage.lastStep : value;
}

void onMapChange()
{
    repairPixlSelection();
    for (u32 i = 0; i < sizeof(sStages) / sizeof(sStages[0]); i++)
        save_data::setGswByte(AP_GSW_JOIN_STAGES + i, static_cast<u8>(stageValue(sStages[i])));
}

}
