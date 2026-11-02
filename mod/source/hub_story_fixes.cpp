#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "hub_story_fixes.h"
#include "pixl_joins.h"
#include "save_data.h"
#include "script_patching.h"
#include "story_progress.h"

namespace mod::hub_story_fixes {

using namespace mod::script_patching;

#define CHAPTER_6_DUELS_DONE 280   // Sammer's Kingdom: the story's duels are done
#define CHAPTER_6_WORLD_ENDED 281  // what vanilla's Flipside Tower scene sets after them
#define CHAPTER_6_FINISHED 286     // Brobot L-type is beaten in the World of Nothing
#define CHAPTER_6_POST_GAME 422    // vanilla's post-game value: Sammer's Kingdom's 100 duels (421+)

// Chapter 6's door in Flipside Tower (mac_02) picks its destination from the story value:
// IF_LARGE_EQUAL(280) { IF_SMALL(421) World of Nothing ELSE Sammer's Kingdom } ELSE Sammer's Kingdom.
// Both IFs read AP_GSW_DOOR_6 instead, which the mod sets from Chapter 6's own value
#define DOOR_6_WORLD_TEST 0x1ad28   // IF_LARGE_EQUAL(GSW(0), 280) -> IF_LARGE_EQUAL(door byte, 1)
#define DOOR_6_POST_GAME_TEST 0x1ad34  // IF_SMALL(GSW(0), 421) -> IF_SMALL(door byte, 2)
#define DOOR_6_SAMMER 0
#define DOOR_6_WORLD_OF_NOTHING 1
#define DOOR_6_POST_GAME 2

// The blue pipe between Flipside Tower (mac_02 aodokan_2) and Flopside Tower (mac_12 aodokan_1): both
// towers hide and switch it off while this flag is clear (relF 0xe94a0 / 0xe95d8)
#define GSWF_TOWER_PIPE 534
#define PIXL_FLEEP 226

#define CASTLE_BLECK_DOOR_MAP "mac_12"  // Flopside Tower
#define CASTLE_BLECK_DOOR_VALUE 357     // its door leads to Chapter 8 for story values 356-358

// ---- Hub tests the low hub value would get wrong ----

// Each test compares the story value with a number; the number becomes one the story value never
// reaches from that side, so the test always comes out the way it did in later vanilla play.
enum TestKind
{
    IF_TEST,    // IF_x(story value, number)
    CASE_TEST,  // CASE_SMALL(number) in a SWITCH on the story value
};

#define NEVER_SMALLER 0   // "story value < 0" and "story value <= -1" are never true
#define NEVER_AT_MOST -1

static const struct
{
    TestKind kind;
    u32 offset;      // bytes from the start of relF's script section
    s32 opcode;      // for IF_TEST: which IF
    s32 value;       // the number vanilla compares with
    s32 replacement;
    const char * what;
} sHubTests[] = {
    // The elevators: switched off (grey button) below these values
    {IF_TEST, 0x1b5d0, OP_IF_SMALL, 53, NEVER_SMALLER, "Flipside 2F elevator (mac_02)"},
    {IF_TEST, 0x24234, OP_IF_SMALL, 73, NEVER_SMALLER, "Flipside 1F elevator (mac_09)"},
    {IF_TEST, 0x33c5c, OP_IF_SMALL, 192, NEVER_SMALLER, "Flopside 1F elevator (mac_19)"},
    // A house door in Flopside 2F, locked below 174
    {IF_TEST, 0x2ca2c, OP_IF_SMALL, 174, NEVER_SMALLER, "Flopside house door (mac_12)"},
    // The Old Key gate in Flipside 2F: locked below 64, gone from 64. It stays open until the Old
    // Key is a location (vanilla gives the key in Peach's join scene, which may not play)
    {IF_TEST, 0x190c4, OP_IF_SMALL, 64, NEVER_SMALLER, "Old Key gate (mac_02)"},
    // The recipe terminals in Flipside and Flopside: "Awaiting data upgrade..." below 64
    {IF_TEST, 0x23c10, OP_IF_SMALL, 64, NEVER_SMALLER, "recipe terminal"},
    // Talking to Peach with the soup at chain value 61 (before Saffron cooked it) walks over a
    // townsperson who never arrives; the "after Saffron" branch works
    {IF_TEST, 0x00434, OP_IF_SMALL_EQUAL, 61, NEVER_AT_MOST, "Peach: soup talk"},
    // Flipside Tower's scene SWITCH: these two cases come before Peach's join scene (CASE_SMALL(63),
    // character_joins) and would catch the hub value first. Their scenes are vanilla's chain steps
    // before 60, which the mod skips
    {CASE_TEST, 0x1bd60, 0, 54, NEVER_SMALLER, "Flipside Tower: scenes below 54"},
    {CASE_TEST, 0x1bd68, 0, 59, NEVER_SMALLER, "Flipside Tower: scenes below 59"},
};

// Points one of door 6's tests at AP_GSW_DOOR_6.
static void patchDoor6Test(u32 offset, s32 opcode, s32 value, s32 replacement, const char * what)
{
    EvtScriptCode * at = relfScript(offset);
    const s32 expected[] = {EVT_CMD(2, opcode), GSW_VAR(0), value};
    if (!matches(at, expected, 3, what))
        return;
    at[1] = GSW_VAR(AP_GSW_DOOR_6);
    at[2] = replacement;
}

void init()
{
    patchDoor6Test(DOOR_6_WORLD_TEST, OP_IF_LARGE_EQUAL, 280, DOOR_6_WORLD_OF_NOTHING, "door 6: World of Nothing");
    patchDoor6Test(DOOR_6_POST_GAME_TEST, OP_IF_SMALL, 421, DOOR_6_POST_GAME, "door 6: post-game");
    for (auto & test : sHubTests)
    {
        EvtScriptCode * at = relfScript(test.offset);
        if (test.kind == IF_TEST)
        {
            const s32 expected[] = {EVT_CMD(2, test.opcode), GSW_VAR(0), test.value};
            if (matches(at, expected, 3, test.what))
                at[2] = test.replacement;
        }
        else
        {
            const s32 expected[] = {EVT_CMD(1, OP_CASE_SMALL), test.value};
            if (matches(at, expected, 2, test.what))
                at[1] = test.replacement;
        }
    }
    wii::os::OSReport("[spmap] hubstory: hub tests patched\n");
}

// ---- Every map change ----

// Whether Chapters 1 to `last` are all finished.
static bool chaptersFinished(s32 last)
{
    for (s32 chapter = 1; chapter <= last; chapter++)
    {
        if (!story_progress::isChapterFinished(chapter))
            return false;
    }
    return true;
}

// Whether Mario is entering Flopside Tower.
static bool enteringFlopsideTower(const char * nextMap)
{
    return story_progress::gStory.context == STORY_CONTEXT_HUB && nextMap != nullptr
        && msl::string::strcmp(nextMap, CASTLE_BLECK_DOOR_MAP) == 0;
}

void onMapChange(const char * nextMap)
{
    if (story_progress::getChapterValue(6) == CHAPTER_6_DUELS_DONE)
        story_progress::advanceChapter(6, CHAPTER_6_WORLD_ENDED);
    // Once Brobot L-type is beaten and Mario has left Chapter 6, its value jumps to the post-game,
    // so Sammer's Kingdom has its 100 duels (and its maps their post-game state). Not while Mario
    // is still in Chapter 6's maps, where the chapter's value is the live one
    s32 chapter6 = story_progress::getChapterValue(6);
    if (chapter6 >= CHAPTER_6_FINISHED && chapter6 < CHAPTER_6_POST_GAME && story_progress::gStory.context != 6)
    {
        story_progress::advanceChapter(6, CHAPTER_6_POST_GAME);
        chapter6 = CHAPTER_6_POST_GAME;
    }
    u8 door6 = DOOR_6_SAMMER;
    if (chapter6 >= CHAPTER_6_POST_GAME)
        door6 = DOOR_6_POST_GAME;
    else if (chapter6 >= CHAPTER_6_DUELS_DONE)
        door6 = DOOR_6_WORLD_OF_NOTHING;
    save_data::setGswByte(AP_GSW_DOOR_6, door6);

    // The blue pipe between the towers works once Mario has Fleep (user, 2026-11-02)
    if (pixl_joins::havePixl(PIXL_FLEEP) && !spm::swdrv::swGet(GSWF_TOWER_PIPE))
    {
        spm::swdrv::swSet(GSWF_TOWER_PIPE);
        wii::os::OSReport("[spmap] hubstory: Fleep found, the blue pipe between the towers works\n");
    }

    if (enteringFlopsideTower(nextMap) && chaptersFinished(7))
    {
        spm::spmario::gp->gsw0 = CASTLE_BLECK_DOOR_VALUE;
        wii::os::OSReport("[spmap] hubstory: Chapters 1-7 finished, Castle Bleck door open in %s\n", nextMap);
    }
}

}
