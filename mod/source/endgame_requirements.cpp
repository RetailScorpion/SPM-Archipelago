#include <common.h>
#include <evt_cmd.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/evt_door.h>
#include <spm/evt_mario.h>
#include <spm/evt_mobj.h>
#include <spm/evt_msg.h>
#include <spm/evt_pouch.h>
#include <spm/evt_sub.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/hitdrv.h>
#include <spm/item_data.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "game_mode.h"
#include "endgame_requirements.h"
#include "game_addresses.h"
#include "mailbox.h"
#include "pure_hearts.h"
#include "received_items.h"
#include "save_data.h"
#include "story_progress.h"

namespace mod::endgame_requirements {

#define TOWER_CHAPTERS 7
#define FINAL_BATTLE_DONE 417        // Chapter 8's value once Count Bleck is beaten

#define THRONE_KEY 23                // the second Goldfish Bowl, renamed
#define THRONE_KEY_ICON 409          // its own picture (RetailScorpion's), added by the patcher (tools/wicon.py)
#define KEY_NAME "ap_throne_key_name"
#define KEY_DESC "ap_throne_key_desc"
#define SIGN_TEXT "ap_throne_sign"

#define STAIRCASE_ROOM "ls4_10"
#define THRONE_DOOR "doa2_l"         // the staircase room's door to the throne room
#define THRONE_DOOR_HIT "A2_doa_02"  // that door's 2D hit object, where the padlock goes

// Where the padlock and sign go: the padlock across from the door's hit object (whose position is the
// door's middle), the sign at a spot measured in game; both at the floor in front of the door (the top
// of the staircase)
#define THRONE_FLOOR_Y 700.0f
#define LOCK_Y 10.0f     // above the floor, like the castle's own padlocks
#define LOCK_Z 2.0f
#define SIGN_X 1033.0f   // left of the door, clear of the foreground pillar (measured in game)
#define SIGN_Z 0.0f

// ---- The requirements ----

static s32 chaptersFinished()
{
    s32 count = 0;
    for (s32 chapter = 1; chapter <= TOWER_CHAPTERS; chapter++)
    {
        if (story_progress::isChapterFinished(chapter))
            count++;
    }
    return count;
}

static s32 heartsHeld()
{
    s32 count = 0;
    for (s32 heart = 1; heart <= PURE_HEART_COUNT; heart++)
    {
        if (spm::swdrv::swGet(AP_GSWF_HEART(heart)))
            count++;
    }
    return count;
}

// Whether a requirement (kind, count) is met.
static bool met(const u32 * requirement)
{
    s32 count = static_cast<s32>(requirement[1]);
    switch (requirement[0])
    {
        case ENDGAME_VANILLA:
            return spm::swdrv::swGet(AP_GSWF_HEART(PURE_HEART_COUNT));
        case ENDGAME_CHAPTERS:
            return chaptersFinished() >= count;
        case ENDGAME_PURE_HEARTS:
            return heartsHeld() >= count;
        case ENDGAME_OPEN:
        default:
            return true;
    }
}

bool castleOpen()
{
    const u32 * requirement = mailbox::gMailbox.castleRequirement;
    if (requirement[0] == ENDGAME_NONE)
        return chaptersFinished() >= TOWER_CHAPTERS;
    return met(requirement);
}

// ---- The padlock and the sign ----

static const s32 sThroneKeyList[] = {THRONE_KEY, -1};  // what the padlock's item menu offers

// Pressing at the padlock: with the key, pick it from the item menu (anything else cancels).
EVT_BEGIN(ap_throne_lock_check)
    USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, THRONE_KEY, LW(0))
    IF_LARGE(LW(0), 0)
        USER_FUNC(spm::evt_sub::evt_sub_item_select_menu, 0, PTR(sThroneKeyList), LW(0), 0)
        IF_NOT_EQUAL(LW(0), THRONE_KEY)
            USER_FUNC(spm::evt_mobj::evt_mobj_exec_cancel, PTR("me"))
        END_IF()
    END_IF()
    RETURN()
EVT_END()

// The padlock opened: remember it, use up the key, and walk through the door.
EVT_BEGIN(ap_throne_lock_open)
    SET(GSWF(AP_GSWF_THRONE_DOOR_OPEN), 1)
    USER_FUNC(spm::evt_pouch::evt_pouch_remove_item, THRONE_KEY)
    USER_FUNC(spm::evt_door::evt_door_enable_disable_map_door_desc, 1, PTR(THRONE_DOOR))
    SET(LW(0), PTR(THRONE_DOOR))
    RUN_CHILD_EVT(game_addresses::door_enter_evt)
    RETURN()
EVT_END()

// Reading the sign.
EVT_BEGIN(ap_throne_sign_read)
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(SIGN_TEXT), 0, 0)
    USER_FUNC(spm::evt_mario::evt_mario_key_on)
    RETURN()
EVT_END()

// ap_throne_place(&needed, &lockX, &lockY, &lockZ, &signX, &signY, &signZ): whether the padlock is
// still needed, and where it and the sign go.
static EVT_DEFINE_USER_FUNC(ap_throne_place)
{
    (void) isFirstCall;
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    bool needed = mailbox::gMailbox.finalRequirement[0] != ENDGAME_NONE  // no client: the battle needs nothing
        && !spm::swdrv::swGet(AP_GSWF_THRONE_DOOR_OPEN)
        && story_progress::getChapterValue(8) < FINAL_BATTLE_DONE;
    wii::mtx::Vec3 door;
    spm::hitdrv::hitObjGetPos(THRONE_DOOR_HIT, &door);
    spm::evtmgr_cmd::evtSetValue(evt, args[0], needed);
    spm::evtmgr_cmd::evtSetFloat(evt, args[1], door.x);
    spm::evtmgr_cmd::evtSetFloat(evt, args[2], THRONE_FLOOR_Y + LOCK_Y);
    spm::evtmgr_cmd::evtSetFloat(evt, args[3], door.z + LOCK_Z);
    spm::evtmgr_cmd::evtSetFloat(evt, args[4], SIGN_X);
    spm::evtmgr_cmd::evtSetFloat(evt, args[5], THRONE_FLOOR_Y);
    spm::evtmgr_cmd::evtSetFloat(evt, args[6], door.z + SIGN_Z);
    wii::os::OSReport("[spmap] endgame: throne door at %d %d %d, padlock %s\n", static_cast<s32>(door.x),
                      static_cast<s32>(door.y), static_cast<s32>(door.z), needed ? "placed" : "not needed");
    return 2;
}
static constexpr s32 ap_throne_place_parameter_count = 7;

EVT_BEGIN(ap_throne_setup)
    USER_FUNC(ap_throne_place, LW(0), LW(1), LW(2), LW(3), LW(4), LW(5), LW(6))
    IF_EQUAL(LW(0), 1)
        USER_FUNC(spm::evt_mobj::evt_mobj_zyo, PTR("ap_throne_lock"), THRONE_KEY, LW(1), LW(2), LW(3), 0,
                  PTR(ap_throne_lock_check), PTR(ap_throne_lock_open), 0)
        USER_FUNC(spm::evt_door::evt_door_enable_disable_map_door_desc, 0, PTR(THRONE_DOOR))
        USER_FUNC(spm::evt_mobj::evt_mobj_kan, 0, PTR("ap_throne_sign"), LW(4), LW(5), LW(6),  // 0: faces the 2D camera
                  PTR(ap_throne_sign_read), 0)
    END_IF()
    RETURN()
EVT_END()

static bool sSetupPending;

void onMapChange(const char * nextMap)
{
    sSetupPending = nextMap != nullptr && msl::string::strcmp(nextMap, STAIRCASE_ROOM) == 0;

    // Not before the client has said what the requirement is (no client: no padlock, no key)
    const u32 * requirement = mailbox::gMailbox.finalRequirement;
    if (requirement[0] != ENDGAME_NONE && !spm::swdrv::swGet(AP_GSWF_THRONE_KEY_GIVEN) && met(requirement))
    {
        spm::swdrv::swSet(AP_GSWF_THRONE_KEY_GIVEN);
        received_items::giveLocal(THRONE_KEY);
        wii::os::OSReport("[spmap] endgame: final requirement met, Throne Room Key given\n");
    }
}

void tick()
{
    if (!sSetupPending)
        return;
    sSetupPending = false;
    spm::evtmgr::evtEntry(ap_throne_setup, 0, 0);
}

// ---- Text ----

// The sign: what's still needed, or that the key opens the door.
static const char * signText()
{
    static char text[256];
    const u32 * requirement = mailbox::gMailbox.finalRequirement;
    s32 count = static_cast<s32>(requirement[1]);
    const char * needed;
    char line[96];
    if (met(requirement))
        needed = "Use the Throne Room Key\nto open the door.";
    else if (requirement[0] == ENDGAME_VANILLA)
        needed = "Find Pure Heart 8\nto get the Throne Room Key.";
    else if (requirement[0] == ENDGAME_CHAPTERS)
    {
        s32 left = count - chaptersFinished();
        msl::stdio::sprintf(line, "Finish %d more chapter%s\nto get the Throne Room Key.", left, left == 1 ? "" : "s");
        needed = line;
    }
    else
    {
        s32 left = count - heartsHeld();
        msl::stdio::sprintf(line, "Find %d more Pure Heart%s\nto get the Throne Room Key.", left, left == 1 ? "" : "s");
        needed = line;
    }
    msl::stdio::sprintf(text, "<kanban>\n<col c00000ff>\n<center>Count Bleck's Throne Room</center>\n</col>\n<center>%s</center>\n<k>",
                        needed);
    return text;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, KEY_NAME) == 0)
        return "Throne Room Key";
    if (msl::string::strcmp(msgName, KEY_DESC) == 0)
        return "It opens the door to Count Bleck's throne room.";
    if (msl::string::strcmp(msgName, SIGN_TEXT) == 0)
        return signText();
    return nullptr;
}

void init()
{
    spm::item_data::ItemData & key = spm::item_data::itemDataTable[THRONE_KEY];
    game_mode::set(&key.nameMsg, static_cast<const char *>(KEY_NAME));
    game_mode::set(&key.descMsg, static_cast<const char *>(KEY_DESC));
    game_mode::writeHalf(&key.iconId, THRONE_KEY_ICON);
}

}
