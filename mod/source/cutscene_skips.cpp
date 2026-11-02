#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/mario_pouch.h>
#include <spm/evt_snd.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "cutscene_skips.h"
#include "mailbox.h"
#include "save_data.h"
#include "script_patching.h"
#include "story_progress.h"

namespace mod::cutscene_skips {

using namespace mod::script_patching;

#define BLECK_SCENE_MAP "ls4_11"  // Castle Bleck's throne room
#define ITEM_RETURN_PIPE 50

// Whether a map is one of the opening's (aa1_*, aa2_*). aa3_01 is the ending and aa4_01 the title
// screen's background, so they don't count.
static bool isOpeningMap(const char * map)
{
    return msl::string::strncmp(map, "aa1_", 4) == 0 || msl::string::strncmp(map, "aa2_", 4) == 0;
}

/*
    A new file: an opening map at story value 0. Vanilla ends the Tippi scene by setting the story
    value to 2 and going to Flipside Tower (mac_02), where Merlon's scenes (story values 2-9) give the
    first Pure Heart and the Return Pipe. All of that is skipped: the file starts in Flipside Tower
    at the prologue's end, and Mario gets the Return Pipe Merlon would have given. Which doors are
    open comes from the seed's Pure Hearts (pure_hearts).
*/
static bool skipOpening(spm::seqdrv::SeqWork * wp)
{
    if (spm::spmario::gp->gsw0 != 0 || !isOpeningMap(wp->p0))
        return false;
    wii::os::OSReport("[spmap] cutscenes: new file, skipping the opening (%s) and Merlon to Flipside Tower\n", wp->p0);
    spm::spmario::gp->gsw0 = STORY_PROLOGUE_END;
    spm::mario_pouch::pouchAddItem(ITEM_RETURN_PIPE);
    wp->p0 = "mac_02";
    wp->p1 = nullptr;
    return true;
}

/*
    The Count Bleck scenes: the game heads to ls4_11 right after the prologue (story value 9) or a
    chapter's ending (its "finished" value). Each entry says where that scene sends Mario at its end.
    After Chapters 1 and 2, that's the end of the Peach / Luigi scene that follows it.
    The chapters' own values aren't changed (the hub ignores what these scenes set); the prologue's
    is, since it carries on into Chapter 1 (the scene sets 10).
*/
static const struct
{
    s32 storyValue;     // the story value when the game heads to the scene
    const char * map;   // where Mario ends up
    const char * door;  // which door he arrives by (nullptr = the map's default spot)
    s32 newValue;       // story value to set, or -1 to leave it
} sBleckScenes[] = {
    {9, "he1_01", "doa1_l", 10},     // prologue -> 1-1
    {53, "mac_02", "doa6_l", -1},    // Chapter 1 -> Bleck (54) -> Peach (55-57)
    {95, "mac_02", "doa8_l", -1},    // Chapter 2 -> Bleck (96) -> Luigi (97-98)
    {125, "mac_02", "doa9_l", -1},   // Chapter 3 -> Bleck (126)
    {173, "mac_02", "doa10_l", -1},  // Chapter 4 -> Bleck (174)
    {220, "mac_02", nullptr, -1},    // Chapter 5 -> Bleck (221)
    {286, "mac_02", "doa12_l", -1},  // Chapter 6 -> Bleck (287)
    {353, "mac_02", nullptr, -1},    // Chapter 7 -> Bleck (354)
};

static bool skipBleckScene(spm::seqdrv::SeqWork * wp)
{
    if (msl::string::strcmp(wp->p0, BLECK_SCENE_MAP) != 0)
        return false;
    s32 & storyValue = spm::spmario::gp->gsw0;
    for (auto & scene : sBleckScenes)
    {
        if (scene.storyValue != storyValue)
            continue;
        wii::os::OSReport("[spmap] cutscenes: skipping the Count Bleck scene at story value %d, to %s\n",
                          storyValue, scene.map);
        if (scene.newValue >= 0)
            storyValue = scene.newValue;
        wp->p0 = scene.map;
        wp->p1 = scene.door;
        return true;
    }
    return false;
}

/*
    King Sammer's scenes around the first Sammer Guy duel (wa1_02). Below Chapter 6 value 228 the room
    plays its scripted version: King Sammer's welcome and the Sammer Guy's challenge (227), the duel,
    then his explanation of the duels (228). Its entry in the story's room table has no values (0, 0),
    since the scenes run that duel, so the room's ordinary version counts its Sammer Guy as beaten.
    When skipping, the room always plays its ordinary version, and its table entry gets the values
    the other rooms have: the duel starts at 227 and winning it sets 228. Without skipping, both go
    back to vanilla.
*/
#define SAMMER_SCENES_CASE 0xd7178    // wa1_02's SWITCH on the story value: CASE_SMALL(228), scripted
#define SAMMER_SCENES_VALUE 228
#define SAMMER_STORY_ROOMS 0xd1938    // the story's Sammer room table (relF section 5), 0x28 per room
#define SAMMER_ROOM_START 3           // words: story value when the duel starts, then when it's won
#define SAMMER_ROOM_WON 4
#define SAMMER_FIRST_START 227
#define SAMMER_FIRST_WON 228

static s32 * sFirstSammerRoom;

static void patchSammerScenes()
{
    caseComparesWithSaveByte(relfScript(SAMMER_SCENES_CASE), SAMMER_SCENES_VALUE, AP_GSW_SAMMER_SCENES,
                             "first Sammer duel: scenes");
    s32 * room = relfScript(SAMMER_STORY_ROOMS);
    const s32 expected[] = {0, 1, 0, 0};  // state, gate 1, no story values
    if (matches(room + 1, expected, 4, "first Sammer duel: story room"))
        sFirstSammerRoom = room;
}

static void setSammerScenes(bool skip)
{
    save_data::setGswByte(AP_GSW_SAMMER_SCENES, skip ? 0 : SAMMER_SCENES_VALUE);
    if (sFirstSammerRoom != nullptr)
    {
        sFirstSammerRoom[SAMMER_ROOM_START] = skip ? SAMMER_FIRST_START : 0;
        sFirstSammerRoom[SAMMER_ROOM_WON] = skip ? SAMMER_FIRST_WON : 0;
    }
}

/*
    Dimentio and Mr. L after Brobot L-type (wa1_27): the script that ends the fight sets Chapter 6 to
    286, then plays their scene, then the save prompt and the map change to Count Bleck's scene (which
    skipBleckScene sends on to Flipside). When skipping, the command after the 286 becomes a jump to a
    label put in place of the short wait before the save prompt. The script has no labels of its own;
    the game collects a script's labels when it starts, so the patch is in place before it runs.
*/
#define JUMP_LABEL 77
#define OP_LBL 3
#define OP_GOTO 4
#define OP_WAIT_FRM 9

static const struct
{
    u32 offset;     // bytes from the start of relF's script section
    s32 count;      // words replaced
    s32 skip[4];    // the words when skipping
    s32 vanilla0;   // the vanilla first word (checked once)
    const char * what;
} sJumpPatches[] = {
    // USER_FUNC(evt_snd_bgmoff_f_d, 2, 3000) -> GOTO(77), WAIT_FRM(0) (never reached)
    {0xdb394, 4, {EVT_CMD(1, OP_GOTO), JUMP_LABEL, EVT_CMD(1, OP_WAIT_FRM), 0}, EVT_CMD(3, OP_USER_FUNC),
     "Brobot L-type: jump past Dimentio and Mr. L"},
    // WAIT_MSEC(500) -> LBL(77)
    {0xdc018, 2, {EVT_CMD(1, OP_LBL), JUMP_LABEL}, EVT_CMD(1, 10), "Brobot L-type: the save prompt"},
};
static s32 sJumpVanilla[2][4];  // each patch's vanilla words, kept to put back
static bool sJumpReady;

static void patchJumps()
{
    for (u32 i = 0; i < sizeof(sJumpPatches) / sizeof(sJumpPatches[0]); i++)
    {
        if (!matches(relfScript(sJumpPatches[i].offset), &sJumpPatches[i].vanilla0, 1, sJumpPatches[i].what))
            return;
    }
    for (u32 i = 0; i < sizeof(sJumpPatches) / sizeof(sJumpPatches[0]); i++)
    {
        const s32 * at = relfScript(sJumpPatches[i].offset);
        for (s32 w = 0; w < sJumpPatches[i].count; w++)
            sJumpVanilla[i][w] = at[w];
    }
    sJumpReady = true;
}

static void setJumps(bool skip)
{
    if (!sJumpReady)
        return;
    for (u32 i = 0; i < sizeof(sJumpPatches) / sizeof(sJumpPatches[0]); i++)
    {
        s32 * at = relfScript(sJumpPatches[i].offset);
        for (s32 w = 0; w < sJumpPatches[i].count; w++)
            at[w] = skip ? sJumpPatches[i].skip[w] : sJumpVanilla[i][w];
    }
}

/*
    Tippi's welcome and pointing tutorial at the start of 1-1 (he1_01). Below story value 12 the map's
    start-up runs the chapter opening (the book page and title card) and hands it Tippi's welcome to
    play after it (story value 12); a line about pointing follows (13), then the tutorial, where
    pointing at the hidden spot reveals Bestovius's house (14). From 14 the start-up leaves the house
    and its door as they are, so when skipping, the welcome is swapped for a script that only starts
    the music and sets 14. The title card still plays. (Tippi can point from story value 9 anyway:
    tippi_always_on.)
*/
#define TIPPI_WELCOME_ARG 0x3af74  // in he1_01's start-up: SET(LW(0), <Tippi's welcome>), its script word
#define TUTORIAL_DONE 14

EVT_BEGIN(ap_skip_tippi_tutorial)
    USER_FUNC(spm::evt_snd::evt_snd_bgmon, 0, PTR("BGM_MAP_STG1"))
    SET(GSW(0), TUTORIAL_DONE)
    RETURN()
EVT_END()

static s32 * sWelcomeArg;
static s32 sWelcomeVanilla;

static void patchTippiTutorial()
{
    s32 * at = relfScript(TIPPI_WELCOME_ARG);
    const s32 expectedHeader[] = {EVT_CMD(2, OP_SET), LW_VAR(0)};
    if (!matches(at - 2, expectedHeader, 2, "Tippi's welcome in 1-1"))
        return;
    sWelcomeArg = at;
    sWelcomeVanilla = *at;
}

static void setTippiTutorial(bool skip)
{
    if (sWelcomeArg != nullptr)
        *sWelcomeArg = skip ? reinterpret_cast<s32>(ap_skip_tippi_tutorial) : sWelcomeVanilla;
}

void init()
{
    patchSammerScenes();
    patchJumps();
    patchTippiTutorial();
}

void onMapChange(spm::seqdrv::SeqWork * wp)
{
    setSammerScenes(!mailbox::gMailbox.playCutscenes);
    setJumps(!mailbox::gMailbox.playCutscenes);
    setTippiTutorial(!mailbox::gMailbox.playCutscenes);
    if (wp->p0 == nullptr || mailbox::gMailbox.playCutscenes)
        return;
    if (!skipOpening(wp))
        skipBleckScene(wp);
}

}
