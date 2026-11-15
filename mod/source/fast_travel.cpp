#include <common.h>
#include <evt_cmd.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/evt_msg.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario.h>
#include <spm/pausewin.h>
#include <spm/seqdrv.h>
#include <spm/spmario.h>
#include <spm/spmario_snd.h>
#include <spm/swdrv.h>
#include <spm/wpadmgr.h>
#include <wii/os.h>

#include "fast_travel.h"
#include "game_mode.h"
#include "pause_chapters.h"
#include "save_data.h"

namespace mod::fast_travel {

#define CHAPTER_COUNT 8
#define SECTION_COUNT 4

// ---- Where each section starts ----

/*
    The tower door's section table (relF evt_mac_door_section_dest, strings at relF section 4 + 0x312c):
    the first section comes in by the door's own entrance, the others by the map's default one.
    Chapter 6's door has no sections; it leads where AP_GSW_DOOR_6 says (hub_story_fixes: the tower
    script's IF at relF 0x1ad28). Chapter 8's door is in Flopside (mac_12): ls<N>_01 by the default
    entrance (not checked against the game's own Flopside door).
*/
struct Section
{
    const char * map;
    const char * entrance;  // nullptr: the map's default
};

static const Section sSections[CHAPTER_COUNT][SECTION_COUNT] = {
    {{"he1_01", "doa1_l"}, {"he2_01", nullptr}, {"he3_01", nullptr}, {"he4_01", nullptr}},
    {{"mi1_01", "doa2_l"}, {"mi2_01", nullptr}, {"mi3_01", nullptr}, {"mi4_01", nullptr}},
    {{"ta1_01", "doa3_l"}, {"ta2_01", nullptr}, {"ta3_01", nullptr}, {"ta4_01", nullptr}},
    {{"sp1_01", "doa4_l"}, {"sp2_01", nullptr}, {"sp3_01", nullptr}, {"sp4_01", nullptr}},
    {{"gn1_01", "doa5_l"}, {"gn2_01", nullptr}, {"gn3_01", nullptr}, {"gn4_01", nullptr}},
    {{nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}},  // Chapter 6: below
    {{"an1_01", "doa7_l"}, {"an2_01", nullptr}, {"an3_01", nullptr}, {"an4_01", nullptr}},
    {{"ls1_01", nullptr}, {"ls2_01", nullptr}, {"ls3_01", nullptr}, {"ls4_01", nullptr}},
};

#define SAMMER_CHAPTER 6
#define DOOR_6_WORLD_OF_NOTHING 1  // AP_GSW_DOOR_6 (hub_story_fixes)
#define DOOR_6_POST_GAME 2

static Section destination(s32 chapter, s32 section)
{
    if (chapter == SAMMER_CHAPTER)
    {
        u8 door = save_data::getGswByte(AP_GSW_DOOR_6);
        bool worldOfNothing = door >= DOOR_6_WORLD_OF_NOTHING && door < DOOR_6_POST_GAME;
        return {worldOfNothing ? "wa1_27" : "wa1_01", "doa5_l"};
    }
    return sSections[chapter - 1][section - 1];
}

// ---- The chapter view ----

/*
    The chapter view (pluswinChapterWinOpen's section state) keeps the chapter's cell in the pluswin work
    at +0x164 (0-7) and the hand's section at +0x168 (0-3). It reads the d-pad at 0x8018AC04 / 0x8018AC40
    and presses at 0x8018AC7C, only for 1 (back to the grid).

    2 on a section the door offers asks the question with the pause menu still open, the way the Return
    Pipe does (user, 2026-11-13): the game's pause menu, when a key item is used (state 0x28, 0x80184DAC),
    drops the system level from 2 to 1 and starts the item's script (evtEntryType(script, 0, 0, 0)); its
    question shows over the menu. Here the mod does the same, and the chapter view's buttons are ignored
    until the script ends. No: back to level 2, the view carries on. Yes: the press becomes a 1, so the
    view goes back to the grid; the grid's + read (0x80189E5C, + closes the whole pause menu: 0x80189E84,
    the windows fade and pluswin work +0x1AC is set, so the pause menu ends with pausewinUnpauseGame) then
    sees + once, and tick changes map once the pause menu is gone.
*/
#define SECTION_LEFT_READ 0x8018AC04   // bl wpadGetButtonsHeldRepeat: the chapter view's d-pad
#define SECTION_RIGHT_READ 0x8018AC40
#define SECTION_PRESS_READ 0x8018AC7C  // bl wpadGetButtonsPressed: the chapter view's 1 (back)
#define GRID_PLUS_READ 0x80189E5C      // bl wpadGetButtonsPressed: the grid's + (close everything)
#define CHAPTER_CELL 0x164
#define SECTION_CELL 0x168
#define BTN_PLUS 0x10
#define BTN_TWO 0x100
#define BTN_ONE 0x200
#define PAUSE_STATE 0x1C               // pausewin work: 0 once the pause menu is closed
#define LEVEL_PAUSED 2                 // spmarioSystemLevel while the pause menu is open
#define LEVEL_ITEM_SCRIPT 1            // while a used item's script runs from it
#define SOUND_CHOOSE "SFX_SYS_MENU_DESIDE1"

enum class Stage
{
    NONE,
    ASKING,        // the question is up over the chapter view
    LEAVING_VIEW,  // Yes: the chapter view goes back to the grid
    CLOSING,       // + was given to the grid; the pause menu is closing
};

static Stage sStage = Stage::NONE;
static s32 sChapter;  // 1-8
static s32 sSection;  // 1-4
static s32 sAskId;    // the question's script
static s32 sAnswer;

static s32 workWord(s32 offset)
{
    return *reinterpret_cast<s32 *>(reinterpret_cast<u8 *>(spm::pausewin::pausewin_pluswinWp) + offset);
}

// ---- The question ----

#define ASK_KEY "ap_fast_travel_ask"
#define CHOICE_KEY "ap_fast_travel_choice"
// The Return Pipe's own question ("modoridokan_02"): No first under the hand
#define CHOICE_TEXT "<select 1 -1 150 40>\nYes\nNo"
#define CHOICE_YES 0

static char sAskText[96];

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, ASK_KEY) == 0)
    {
        msl::stdio::sprintf(sAskText, "<system>\nWould you like to fast travel\nto %d-%d?\n<o>", sChapter, sSection);
        return sAskText;
    }
    if (msl::string::strcmp(msgName, CHOICE_KEY) == 0)
        return CHOICE_TEXT;
    return nullptr;
}

// ap_fast_travel_answer(choice): keeps the answer for the chapter view.
static EVT_DEFINE_USER_FUNC(ap_fast_travel_answer)
{
    (void) isFirstCall;
    sAnswer = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    return 2;
}
static constexpr s32 ap_fast_travel_answer_parameter_count = 1;

EVT_BEGIN(ap_fast_travel_evt)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(ASK_KEY), 0, 0)
    USER_FUNC(spm::evt_msg::evt_msg_select, 0, PTR(CHOICE_KEY))
    USER_FUNC(spm::evt_msg::evt_msg_continue)
    USER_FUNC(ap_fast_travel_answer, LW(0))
    RETURN()
EVT_END()

// ---- The hooks ----

static u32 sectionMove(s32 channel)
{
    return sStage == Stage::NONE ? spm::wpadmgr::wpadGetButtonsHeldRepeat(channel) : 0;
}

static u32 sectionPress(s32 channel)
{
    if (sStage == Stage::ASKING)
    {
        if (spm::evtmgr::evtCheckID(sAskId))
            return 0;  // still asking
        spm::spmario::spmarioSystemLevel(LEVEL_PAUSED);
        if (sAnswer != CHOICE_YES)
        {
            sStage = Stage::NONE;
            return 0;
        }
        sStage = Stage::LEAVING_VIEW;
        return BTN_ONE;
    }
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(channel);
    if (!(pressed & BTN_TWO) || sStage != Stage::NONE)
        return pressed;
    s32 chapter = workWord(CHAPTER_CELL) + 1;
    s32 section = workWord(SECTION_CELL) + 1;
    if (chapter < 1 || chapter > CHAPTER_COUNT || section < 1 || section > SECTION_COUNT ||
        !pause_chapters::sectionReachable(chapter, section))
        return pressed;
    sChapter = chapter;
    sSection = section;
    sAnswer = -1;
    spm::spmario_snd::spsndSFXOn(SOUND_CHOOSE);
    spm::spmario::spmarioSystemLevel(LEVEL_ITEM_SCRIPT);
    sAskId = spm::evtmgr::evtEntryType(ap_fast_travel_evt, 0, 0, 0)->id;
    sStage = Stage::ASKING;
    return pressed & ~BTN_TWO;
}

static u32 gridPlusPress(s32 channel)
{
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(channel);
    if (sStage != Stage::LEAVING_VIEW)
        return pressed;
    sStage = Stage::CLOSING;
    return pressed | BTN_PLUS;
}

/*
    Leaving Chapter 4 by fast travel kept the Helmet and Squirps on Mario in other chapters (user,
    2026-11-14). The Return Pipe's script (main.dol 0x803C04A0) takes them off before its map change:
    evt_mario_take_damage+0x120(0) -> 0x8012CE14 (Squirps' fire mode off: miscFlags 0x200000),
    evt_mario_take_damage+0x2F4(0) -> 0x8012CEEC (miscFlags 0x80000 off), and
    evt_mario_flag8_onoff(0, 0x400000) (MarioWork +8). Fast travel does the same.
*/
#define SQUIRPS_OFF 0x8012CE14
#define HELMET_MODE_OFF 0x8012CEEC
#define MARIO_FLAGS_8 0x8
#define FLAG_8_SPACE 0x400000

static void leaveSpaceMode()
{
    reinterpret_cast<void (*)()>(SQUIRPS_OFF)();
    reinterpret_cast<void (*)()>(HELMET_MODE_OFF)();
    u32 & flags = *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(spm::mario::marioGetPtr()) + MARIO_FLAGS_8);
    flags &= ~FLAG_8_SPACE;
}

/*
    Doing that before the map change wasn't enough (user, 2026-11-14: Luigi in 1-1 with the Helmet and
    Squirps; MarioWork miscFlags 0x200000 and +8 0x400000 still set there). The map change
    (seq_mapChangeMain, 0x80179F30) only sets the gear from its saved states (always 0 = off; only
    0x8017B40C writes them) while gp flag 0x2 is set, and the space rooms can turn the gear back on in the
    frames before the map changes. So fast travel also sets gp flag 0x2 and takes the gear off again on
    the first frame in the new room (not for Chapter 4, whose rooms put it on themselves).
*/
#define GP_KEEP_GEAR_FLAG 0x2   // gp->flags: the map change sets Squirps / Helmet from its saved states
#define SPACE_CHAPTER 4
// Neither was it (user, 2026-11-14): the gear is also kept in save flags, which the room setup puts
// back on Mario (0x800E4BF4-0x800E4C28: GSWF 401 -> Squirps on, 402 -> Helmet on; 403 is Tippi). Fast travel
// out of Chapter 4 clears both.
#define GSWF_SQUIRPS_ON 401
#define GSWF_HELMET_ON 402

static const char * sArrivalMap;  // the map fast travel is going to, until its first frame

void tick()
{
    if (sArrivalMap != nullptr && msl::string::strcmp(spm::spmario::gp->mapName, sArrivalMap) == 0)
    {
        leaveSpaceMode();
        sArrivalMap = nullptr;
    }
    if (sStage != Stage::LEAVING_VIEW && sStage != Stage::CLOSING)
        return;
    bool paused = *reinterpret_cast<s32 *>(reinterpret_cast<u8 *>(spm::pausewin::pausewin_wp) + PAUSE_STATE) != 0;
    if (paused)
        return;
    Stage stage = sStage;
    sStage = Stage::NONE;
    if (stage != Stage::CLOSING)
        return;  // the pause menu closed some other way
    Section to = destination(sChapter, sSection);
    wii::os::OSReport("[spmap] fast travel: %d-%d, %s (%s)\n", sChapter, sSection, to.map,
                      to.entrance ? to.entrance : "default entrance");
    leaveSpaceMode();
    if (sChapter != SPACE_CHAPTER)
    {
        spm::swdrv::swClear(GSWF_SQUIRPS_ON);
        spm::swdrv::swClear(GSWF_HELMET_ON);
    }
    spm::spmario::gp->flags |= GP_KEEP_GEAR_FLAG;
    sArrivalMap = sChapter == SPACE_CHAPTER ? nullptr : to.map;
    spm::seqdrv::seqSetSeq(spm::seqdrv::SEQ_MAPCHANGE, to.map, to.entrance);
}

void init()
{
    game_mode::writeBranch(reinterpret_cast<void *>(SECTION_LEFT_READ), reinterpret_cast<void *>(sectionMove), true);
    game_mode::writeBranch(reinterpret_cast<void *>(SECTION_RIGHT_READ), reinterpret_cast<void *>(sectionMove), true);
    game_mode::writeBranch(reinterpret_cast<void *>(SECTION_PRESS_READ), reinterpret_cast<void *>(sectionPress), true);
    game_mode::writeBranch(reinterpret_cast<void *>(GRID_PLUS_READ), reinterpret_cast<void *>(gridPlusPress), true);
}

}
