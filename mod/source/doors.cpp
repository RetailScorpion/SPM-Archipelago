#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_cam.h>
#include <spm/evt_door.h>
#include <spm/evt_hit.h>
#include <spm/evt_map.h>
#include <spm/evt_mario.h>
#include <spm/evt_msg.h>
#include <spm/evtmgr_cmd.h>
#include <spm/msgdrv.h>
#include <spm/swdrv.h>
#include <wii/os.h>
#include <msl/stdio.h>
#include <msl/string.h>

#include "apitem.h"
#include "doors.h"
#include "gamesyms.h"
#include "mailbox.h"
#include "patch.h"
#include "savedata.h"
#include "storyvalue.h"

namespace mod::doors {

using namespace spm::evt_cam;
using namespace spm::evt_door;
using namespace spm::evt_hit;
using namespace spm::evt_map;
using namespace spm::evt_mario;
using namespace spm::evt_msg;
using namespace mod::gamesyms;

#define TOWER_DOOR_COUNT 7  // chapters 1-7; Chapter 8's door is in Flopside
#define SAMMER_DOOR 5       // Chapter 6's door only ever offers its first entry (vanilla)

/*
    Section-select menus

    Vanilla uses one fixed message per chapter listing all four sections. The mod gives each door
    its own message key ("ap_door_sel_N") and builds the text on demand, listing only the sections
    that chapter has reached, so the menu works mid-chapter.
*/

#define SELECT_KEY_PREFIX "ap_door_sel_"

static const char * sSelectKeys[TOWER_DOOR_COUNT] = {
    SELECT_KEY_PREFIX "1", SELECT_KEY_PREFIX "2", SELECT_KEY_PREFIX "3", SELECT_KEY_PREFIX "4",
    SELECT_KEY_PREFIX "5", SELECT_KEY_PREFIX "6", SELECT_KEY_PREFIX "7",
};
static char sSelectText[TOWER_DOOR_COUNT][128];

static s32 sectionCount(s32 door)
{
    return storyvalue::sectionsReached(door + 1);
}

static const char * buildSelectText(s32 door)
{
    char * out = sSelectText[door];
    s32 n = msl::stdio::sprintf(out, "<select 0 -2 280 60>\n");
    for (s32 i = 0; i < sectionCount(door); i++)
        n += msl::stdio::sprintf(out + n, "Chapter %d-%d\n", door + 1, i + 1);
    msl::stdio::sprintf(out + n, "None of them");
    return out;
}

static const char * (*msgSearchReal)(const char * msgName);
// The mod's one msgSearch hook (a function can only be hooked once): the AP item's messages, then
// the door menus
static const char * msgSearchHook(const char * msgName)
{
    if (const char * text = apitem::message(msgName))
        return text;
    if (msl::string::strncmp(msgName, SELECT_KEY_PREFIX, sizeof(SELECT_KEY_PREFIX) - 1) == 0)
    {
        s32 door = msgName[sizeof(SELECT_KEY_PREFIX) - 1] - '1';
        if (door >= 0 && door < TOWER_DOOR_COUNT)
            return buildSelectText(door);
    }
    return msgSearchReal(msgName);
}

/*
    Pure Hearts
*/

bool isHeartItem(s32 itemId)
{
    return itemId >= PURE_HEART_ITEM_FIRST && itemId < PURE_HEART_ITEM_FIRST + PURE_HEART_COUNT;
}

void giveHeart(s32 heart)
{
    spm::swdrv::swSet(AP_GSWF_HEART(heart));
    wii::os::OSReport("[spmap] doors: Pure Heart %d received\n", heart);
}

bool isDoorOpen(s32 heart)
{
    return mailbox::gMailbox.startingHearts == STARTING_HEARTS_NO_CLIENT || spm::swdrv::swGet(AP_GSWF_HEART(heart));
}

void onMapChange()
{
    u32 starting = mailbox::gMailbox.startingHearts;
    if (starting == STARTING_HEARTS_NO_CLIENT)
        return;
    for (s32 heart = 1; heart <= PURE_HEART_COUNT; heart++)
    {
        if ((starting & (1 << (heart - 1))) && !spm::swdrv::swGet(AP_GSWF_HEART(heart)))
        {
            spm::swdrv::swSet(AP_GSWF_HEART(heart));
            wii::os::OSReport("[spmap] doors: starting with Pure Heart %d\n", heart);
        }
    }
}

/*
    Script functions
*/

// ap_door_has_select(door, &result): the door gets the section menu (two or more sections reached)
static EVT_DEFINE_USER_FUNC(ap_door_has_select)
{
    (void) isFirstCall;
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    s32 door = spm::evtmgr_cmd::evtGetValue(evt, args[0]);
    spm::evtmgr_cmd::evtSetValue(evt, args[1], sectionCount(door) >= 2);
    return 2;
}
static constexpr s32 ap_door_has_select_parameter_count = 2;

// ap_door_select_msg(door, &key): the mod's section-list message key for the door
static EVT_DEFINE_USER_FUNC(ap_door_select_msg)
{
    (void) isFirstCall;
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    s32 door = spm::evtmgr_cmd::evtGetValue(evt, args[0]);
    spm::evtmgr_cmd::evtSetValue(evt, args[1], reinterpret_cast<s32>(sSelectKeys[door]));
    return 2;
}
static constexpr s32 ap_door_select_msg_parameter_count = 2;

// ap_door_valid_choice(key, choice, &result): choice is a listed section (not "None of them")
static EVT_DEFINE_USER_FUNC(ap_door_valid_choice)
{
    (void) isFirstCall;
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    const char * key = reinterpret_cast<const char *>(spm::evtmgr_cmd::evtGetValue(evt, args[0]));
    s32 choice = spm::evtmgr_cmd::evtGetValue(evt, args[1]);
    s32 door = key[sizeof(SELECT_KEY_PREFIX) - 1] - '1';
    s32 max = door == SAMMER_DOOR ? 1 : sectionCount(door);
    spm::evtmgr_cmd::evtSetValue(evt, args[2], choice >= 0 && choice < max);
    return 2;
}
static constexpr s32 ap_door_valid_choice_parameter_count = 3;

// ap_door_open(door, &result): the door is open (its Pure Heart is held)
static EVT_DEFINE_USER_FUNC(ap_door_open)
{
    (void) isFirstCall;
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    s32 door = spm::evtmgr_cmd::evtGetValue(evt, args[0]);
    spm::evtmgr_cmd::evtSetValue(evt, args[1], isDoorOpen(door + 1));
    return 2;
}
static constexpr s32 ap_door_open_parameter_count = 2;

/*
    Copy of mac_door_select_evt (relF 0xB8DCC), run when a section-select door is used.
    LW(0) = map door desc, LW(1) = door message, LW(2) = section list message.
    Change: which choices are sections comes from the mod's list, not a fixed 0-3.
*/
EVT_BEGIN(ap_door_select_evt)
    SET(LW(10), LW(0))
    SET(LW(11), LW(1))
    SET(LW(12), LW(2))
    USER_FUNC(evt_mario_key_off, 0)
    USER_FUNC(evt_mario_fairy_reset)
    USER_FUNC(evt_cam_look_at_door, 1, 0)
    USER_FUNC(evt_msg_print, 0, LW(11), 0, 0)
    USER_FUNC(evt_msg_select, 0, LW(12))
    USER_FUNC(evt_msg_continue)
    USER_FUNC(ap_door_valid_choice, LW(12), LW(0), LW(3))
    IF_NOT_EQUAL(LW(3), 0)
        USER_FUNC(evt_mac_door_section_dest, LW(10), LW(0), LW(1), LW(2))
        USER_FUNC(evt_mac_set_door_dest, LW(10), LW(1), LW(2))
        SET(LW(0), LW(10))
        RUN_CHILD_EVT(door_enter_evt)
    END_IF()
    SET(LW(0), LW(10))
    SET(LW(1), LW(11))
    SET(LW(2), LW(12))
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

/*
    Copy of mac_door_setup_evt (relF 0xB906C). Changes:
    - which doors get the section menu: every door whose chapter has reached two or more sections
      (vanilla: every finished chapter, judged from the hub's single story value)
    - the menu uses the mod's message and select script
    - a door is open when its Pure Heart is held (isDoorOpen). Vanilla opens the first N by the hub's
      story value. LW(0) stays the vanilla count of 7, which the menu setup uses.
      Chapter 8's door is in Flopside and isn't affected.
*/
EVT_BEGIN(ap_door_setup_evt)
    // Vanilla: number of open doors (0-7 from the hub value)
    SET(LW(0), TOWER_DOOR_COUNT)
    SET(LW(1), 0)
    DO(7)
        USER_FUNC(ap_door_open, LW(1), LF(0))
        USER_FUNC(evt_mac_door_info, LW(1), LW(2), LW(3), LW(4), LW(5), LW(6))
        IF_NOT_EQUAL(LF(0), 0)
            USER_FUNC(ap_door_has_select, LW(1), LW(8))
            SET(LF(1), LW(8))
            SET(LW(7), LW(0))
            SUB(LW(7), 1)
            IF_EQUAL(LF(1), 0)
                USER_FUNC(evt_door_enable_disable_map_door_desc, 1, LW(2))
            ELSE()
                USER_FUNC(evt_door_enable_disable_map_door_desc, 0, LW(2))
                USER_FUNC(evt_mac_door_msgs, LW(1), LW(8), LW(9))
                USER_FUNC(ap_door_select_msg, LW(1), LW(9))
                USER_FUNC(evt_door_select_entry, 5, 1, LW(3), LW(4), PTR(ap_door_select_evt), LW(7))
                USER_FUNC(evt_door_select_set, LW(7), 0, LW(2))
                USER_FUNC(evt_door_select_set, LW(7), 1, LW(8))
                USER_FUNC(evt_door_select_set, LW(7), 2, LW(9))
            END_IF()
            USER_FUNC(evt_hitobj_onoff, LW(3), 1, 1)
            USER_FUNC(evt_hitobj_onoff, LW(4), 1, 1)
            USER_FUNC(evt_mapobj_flag_onoff, 1, 0, LW(5), 1)
            USER_FUNC(evt_map_playanim, LW(6), 2, 0)
            USER_FUNC(evt_mac_door_objs, LW(1), LW(10), LW(11))
            IF_EQUAL(LF(1), 0)
                USER_FUNC(evt_mapobj_flag_onoff, 1, 1, LW(11), 1)
            END_IF()
        ELSE()
            USER_FUNC(evt_door_enable_disable_map_door_desc, 0, LW(2))
            USER_FUNC(evt_mapobj_flag_onoff, 1, 1, LW(5), 1)
            USER_FUNC(evt_mac_door_objs, LW(1), LW(10), LW(11))
            USER_FUNC(evt_mapobj_flag_onoff, 1, 0, LW(10), 1)
            USER_FUNC(evt_mapobj_flag_onoff, 1, 1, LW(11), 1)
        END_IF()
        ADD(LW(1), 1)
    WHILE()
    RETURN()
EVT_END()

void init()
{
    msgSearchReal = patch::hookFunction(spm::msgdrv::msgSearch, msgSearchHook);

    // mac_tower_run_door_setup is RUN_CHILD_EVT(mac_door_setup_evt): opcode word, then the script
    if (mac_tower_run_door_setup[1] != reinterpret_cast<s32>(mac_door_setup_evt))
    {
        wii::os::OSReport("[spmap] doors: tower script isn't what was expected, not patched\n");
        return;
    }
    mac_tower_run_door_setup[1] = reinterpret_cast<s32>(ap_door_setup_evt);
    wii::os::OSReport("[spmap] doors: tower door setup redirected\n");
}

}
