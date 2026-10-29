#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_door.h>
#include <spm/evt_map.h>
#include <spm/evt_mario.h>
#include <spm/evt_npc.h>
#include <spm/evt_pouch.h>
#include <spm/evt_shop.h>
#include <spm/evt_snd.h>
#include <spm/evtmgr_cmd.h>
#include <spm/item_data_ids.h>
#include <spm/mario_pouch.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "apitem.h"
#include "delivery.h"
#include "gamesyms.h"
#include "hubstory.h"
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
#define OP_IF_LARGE 27
#define OP_IF_SMALL_EQUAL 28
#define OP_IF_LARGE_EQUAL 29
#define OP_SWITCH 34
#define OP_SET 50
#define OP_CASE_SMALL 38
#define OP_RUN_EVT 93
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
#define PIXL_PICCOLO 229
#define PIXL_BARRY 230
#define PIXL_DASHELL 231

// Characters as evt_mario_set_character takes them; their items are CHARACTER_ITEM_BASE + character
#define CHARACTER_PEACH 1
#define CHARACTER_BOWSER 2
#define CHARACTER_LUIGI 3
#define CHARACTER_ITEM_BASE 216

#define ITEM_SPICY_SOUP 165
#define NPC_PROPERTY_TALK 9  // evt_npc_set_property: the script run when Mario talks to the NPC

// Location ids the joins report to the client: JOIN_LOCATION_BASE + Pixl item id (stand-ins until
// Archipelago location ids exist). Bit JOIN_LOCATION_OTHER_WORLD marks "held another world's item".
#define JOIN_LOCATION_BASE 0x20000
#define JOIN_LOCATION_OTHER_WORLD 0x1000

// What a join gives with DEBUG_JOINS_GIVE_PLACEHOLDER when the client hasn't picked an item
#define DEFAULT_PLACEHOLDER_ITEM 71  // Gold Bar

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
    GIFT,       // USER_FUNC(evt_pouch_add_item, a) -> USER_FUNC(ap_join_gift, a), and the
                // USER_FUNC(evt_pouch_set_pixl_selected, a) right after -> USER_FUNC(ap_join_select, a)
    SAFETY_NET, // IF_EQUAL(LW(0), 0) -> never true
    STAGE,      // SWITCH(GSW(0)) -> SWITCH(GSW(AP_GSW_JOIN_STAGES + a))
    CASE_NEVER, // CASE_SMALL(a) -> CASE_SMALL(b), where b makes it unreachable
    HAVE_NEVER, // IF_NOT_EQUAL(LW(0), 0) -> IF_NOT_EQUAL(LW(0), LW(0)): never true
    SKIP,       // USER_FUNC(sSkippable[b], ...) with a args -> USER_FUNC(ap_skip, ...): does nothing
    FLEEP_ONLY, // USER_FUNC(sFleepOnly[b].real, x) -> USER_FUNC(sFleepOnly[b].wrapper, x): runs only with Fleep
    CHAR_JOIN,  // USER_FUNC(evt_mario_set_character, a) -> USER_FUNC(ap_char_join, a): collects the
                // character's location (gift b), then switches to the character only if Mario has them
    GIFT_GATE,  // IF_LARGE_EQUAL(GSW(0), a) -> IF_EQUAL(GSWF(gift location b), 0)
    ALWAYS,     // IF_SMALL(GSW(0), a) -> IF_SMALL(GSW(0), STORY_VALUE_NEVER): always true
    CASE_GSW,   // CASE_SMALL(a) -> CASE_SMALL(GSW(b)): a save byte the mod sets (hubstory, tick)
    LESSON_IF,  // IF with opcode b on (GSW(0), a) -> the same on GSW(AP_GSW_LESSON), set by hubstory
    LESSON,     // SET(GSW(0), a) -> USER_FUNC(ap_lesson, a): collects Bestovius's lesson location
    PEACH,      // instruction b on GSW(0) (with constant a, unless b is a SWITCH) -> the same on
                // GSW(AP_GSW_PEACH_CHAIN), Peach's revival chain's own story value (hubstory)
    PEACH_JOIN, // RUN_EVT(Peach's join scene at offset a) -> RUN_EVT(ap_peach_join_evt)
    PEACH_TALK, // evt_npc_set_property(npc, 9 (talk), script at offset a) -> the talk script is
                // ap_peach_soup_evt, which runs that script only for the scenes
};

// Past every story value, for ALWAYS
#define STORY_VALUE_NEVER 30000

// Functions a SKIP patch may replace
enum Skippable
{
    DOOR_ENABLE_DISABLE,
    SFX_3D,
    MAP_PLAYANIM,
    POUCH_ADD_ITEM,
};
static const void * const sSkippable[] = {
    reinterpret_cast<const void *>(spm::evt_door::evt_door_enable_disable_map_door_desc),
    reinterpret_cast<const void *>(spm::evt_snd::evt_snd_sfxon_3d),
    reinterpret_cast<const void *>(spm::evt_map::evt_map_playanim),
    reinterpret_cast<const void *>(spm::evt_pouch::evt_pouch_add_item),
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
    // ta2_04 is a dead end: bars close behind Mario and only ground-pounding the pile (needs Thudley)
    // opens them, and the Return Pipe is always off there. The bars now never close or reopen; the
    // pile still sets the story value to 108, so progress past here still needs Thudley
    {SKIP, 0x7f050, 2, DOOR_ENABLE_DISABLE, "Thudley: door barred (trap)"},
    {SKIP, 0x7f078, 4, SFX_3D, "Thudley: bars closing sound"},
    {SKIP, 0x7f090, 3, MAP_PLAYANIM, "Thudley: bars closing"},
    {SKIP, 0x7f360, 4, SFX_3D, "Thudley: bars opening sound"},
    {SKIP, 0x7f378, 3, MAP_PLAYANIM, "Thudley: bars opening"},

    {GATE, 0x8f8a0, 123, PIXL_CARRIE, "Carrie: her and the cage"},
    {GIFT, 0x8f684, PIXL_CARRIE, 0, "Carrie join"},
    {SAFETY_NET, 0x91278, 0, 0, "Carrie safety net"},

    {STAGE, 0x9b328, 2, 0, "Fleep: outhouse stage"},
    {GIFT, 0x9beec, PIXL_FLEEP, 0, "Fleep join"},
    {FLEEP_ONLY, 0x9c2e4, 0, 0, "Fleep: pointer mode on (crashes without him)"},
    {FLEEP_ONLY, 0x9c2f0, 0, 1, "Fleep: pointer mode set (crashes without him)"},

    {GATE, 0xb9ee4, 192, PIXL_CUDGE, "Cudge: him"},
    {GATE, 0xb9fe8, 192, PIXL_CUDGE, "Cudge: tablet slots"},
    {CASE_NEVER, 0xb9f8c, 193, 189, "Cudge: exit pipe off at 189-192 (trap)"},
    {GIFT, 0xb9218, PIXL_CUDGE, 0, "Cudge join"},

    {GATE, 0xc7050, 209, PIXL_DOTTIE, "Dottie: her"},
    {GIFT, 0xc6e0c, PIXL_DOTTIE, 0, "Dottie join"},
    {SAFETY_NET, 0xcdffc, 0, 0, "Dottie safety net"},

    // Optional Pixls: their scenes aren't gated by story value (tested), so only the gift is patched
    {GIFT, 0x33684, PIXL_PICCOLO, 0, "Piccolo join"},
    {GIFT, 0x7b024, PIXL_BARRY, 0, "Barry join"},
    {GIFT, 0x15b6d4, PIXL_DASHELL, 0, "Dashell join"},

    // Characters: the join scenes collect a gift location (hubstory.cpp) instead of adding the
    // character. Their own evt_pouch_add_item is skipped; ap_char_join gives what was placed there.
    // Peach: her revival chain reads and sets its own story value (AP_GSW_PEACH_CHAIN, 60-63) instead
    // of GSW(0): mac_01 shows her unconscious with Merlon (60-62) and plays the "she fell from the
    // sky" scene (60 -> 61), Saffron cooks the Spicy Soup from a Fire Burst (61 -> 62, sSaffronPatches).
    // Talking to her with the soup (ap_peach_soup_evt) collects her location (63); with cutscenes on
    // and Peach in the party it runs vanilla's instead, which takes Mario to Merlon's house, where the
    // join scene plays (62 -> 63)
    {PEACH, 0x02f8c, 60, EVT_CMD(2, OP_IF_LARGE_EQUAL), "Peach: mac_01 Peach and Merlon (from)"},
    {PEACH, 0x02f98, 63, EVT_CMD(2, OP_IF_SMALL), "Peach: mac_01 Peach and Merlon (to)"},
    {PEACH, 0x0317c, 60, EVT_CMD(2, OP_IF_LARGE), "Peach: mac_01 Merlon's spot"},
    {PEACH, 0x036e4, 0, EVT_CMD(1, OP_SWITCH), "Peach: mac_01 unconscious scene trigger"},
    {PEACH, 0x02908, 61, EVT_CMD(2, OP_SET), "Peach: mac_01 unconscious scene end"},
    {PEACH_TALK, 0x03168, 0x00230, 0, "Peach: talk with the Spicy Soup"},
    // (Giving the soup at 61, before Saffron, walks a townsperson over who never arrives there: that
    // check keeps reading the hub value, so it takes the "after Saffron" branch)
    {PEACH, 0x19c78, 61, EVT_CMD(2, OP_IF_LARGE_EQUAL), "Peach: mac_02 scene actors (from)"},
    {PEACH, 0x19c84, 63, EVT_CMD(2, OP_IF_SMALL), "Peach: mac_02 scene actors (to)"},
    {PEACH, 0x0d4ec, 63, EVT_CMD(2, OP_SET), "Peach: join scene end"},
    // mac_02 SWITCHes for the scene whose other cases the hub value never reaches: who counts as
    // inside Merlon's house (61-62 adds the Mario NPC the scene waits on), its music and its sound
    {PEACH, 0x1aee0, 0, EVT_CMD(1, OP_SWITCH), "Peach: Merlon's house NPCs"},
    {PEACH, 0x1bab4, 0, EVT_CMD(1, OP_SWITCH), "Peach: scene music"},
    {PEACH, 0x1bb6c, 0, EVT_CMD(1, OP_SWITCH), "Peach: scene sound"},
    {PEACH_JOIN, 0x1bdf4, 0xca08, 0, "Peach: join scene without Peach"},
    // The join scene runs for CASE_SMALL(63) of mac_02's story value SWITCH (hub value), entering from
    // Merlon's house; the CASE compares with a value hubstory sets once the chain reaches 62
    {CASE_GSW, 0x1bdd4, 63, AP_GSW_PEACH_SCENE, "Peach: join scene trigger"},
    {SKIP, 0xcaf8, 1, POUCH_ADD_ITEM, "Peach: vanilla join"},
    {CHAR_JOIN, 0xcb04, CHARACTER_PEACH, hubstory::GIFT_PEACH, "Peach join"},
    // Bowser: 3-1 (ta1_04) after his fight; the fight still waits for Chapter 3's own value (< 104)
    {SKIP, 0x74744, 1, POUCH_ADD_ITEM, "Bowser: vanilla join"},
    {CHAR_JOIN, 0x74724, CHARACTER_BOWSER, hubstory::GIFT_BOWSER, "Bowser join"},
    // Luigi: the Underwhere (an1_07) plays his scene for story values 295-296, before Chapter 7's start
    {GIFT_GATE, 0xede0c, 295, hubstory::GIFT_LUIGI, "Luigi: join scene"},
    {ALWAYS, 0xede18, 297, 0, "Luigi: join scene (upper bound)"},
    {SKIP, 0xeda4c, 1, POUCH_ADD_ITEM, "Luigi: vanilla join"},
    {CHAR_JOIN, 0xeda20, CHARACTER_LUIGI, hubstory::GIFT_LUIGI, "Luigi join"},

    // Watchitt (he2_03) sends for the bridge builder at Chapter 1's value 25 (Thoreau's join scene
    // played) and says "go get help" below it. His "go get help" CASE now compares with a byte tick()
    // keeps from whether Mario has Thoreau, so the bridge scene plays once he does, whatever the value
    {CASE_GSW, 0x40f1c, 25, AP_GSW_WATCHITT, "Watchitt: waits for Thoreau"},

    // Bestovius (he1_06): talking to him gives the flip lesson below Chapter 1's story value 15 and
    // only chats from 16 on. His talk script now asks a value hubstory sets from the lesson's own
    // location flag, so the lesson stays available whatever Chapter 1's value is
    {LESSON_IF, 0x1645f8, 421, OP_IF_LARGE_EQUAL, "Bestovius: chat 421"},
    {LESSON_IF, 0x164628, 354, OP_IF_LARGE_EQUAL, "Bestovius: chat 354"},
    {LESSON_IF, 0x164658, 287, OP_IF_LARGE_EQUAL, "Bestovius: chat 287"},
    {LESSON_IF, 0x164688, 221, OP_IF_LARGE_EQUAL, "Bestovius: chat 221"},
    {LESSON_IF, 0x1646b8, 174, OP_IF_LARGE_EQUAL, "Bestovius: chat 174"},
    {LESSON_IF, 0x1646e8, 126, OP_IF_LARGE_EQUAL, "Bestovius: chat 126"},
    {LESSON_IF, 0x164718, 96, OP_IF_LARGE_EQUAL, "Bestovius: chat 96"},
    {LESSON_IF, 0x164748, 17, OP_IF_LARGE_EQUAL, "Bestovius: chat 17"},
    {LESSON_IF, 0x164778, 16, OP_IF_LARGE_EQUAL, "Bestovius: chat 16"},
    {LESSON_IF, 0x1647a8, 15, OP_IF_SMALL, "Bestovius: first offer"},
    {LESSON, 0x164df0, 16, 0, "Bestovius: lesson"},
};

// Saffron's cooking script (main.dol), offsets from saffronCookingScript: her Spicy Soup for Peach
// (story values 61-62) follows Peach's chain value
static const Patch sSaffronPatches[] = {
    {PEACH, 0x020, 61, EVT_CMD(2, OP_IF_EQUAL), "Saffron: first visit"},
    {PEACH, 0x150, 62, EVT_CMD(2, OP_SET), "Saffron: soup already held (first visit)"},
    {PEACH, 0x200, 62, EVT_CMD(2, OP_SET), "Saffron: soup already held"},
    {PEACH, 0x284, 62, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup cooked"},
    {PEACH, 0x40c, 62, EVT_CMD(2, OP_IF_EQUAL), "Saffron: menu cancelled"},
    {PEACH, 0x45c, 61, EVT_CMD(2, OP_IF_LARGE_EQUAL), "Saffron: only a Fire Burst (from)"},
    {PEACH, 0x468, 62, EVT_CMD(2, OP_IF_SMALL_EQUAL), "Saffron: only a Fire Burst (to)"},
    {PEACH, 0x54c, 0, EVT_CMD(1, OP_SWITCH), "Saffron: cooking declined"},
    {PEACH, 0x5a8, 0, EVT_CMD(1, OP_SWITCH), "Saffron: cooking line"},
    {PEACH, 0x858, 0, EVT_CMD(1, OP_SWITCH), "Saffron: cooking message"},
    {PEACH, 0xa1c, 61, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup result (61)"},
    {PEACH, 0xa5c, 62, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup result (62)"},
    {PEACH, 0xb88, 61, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup handed over (61)"},
    {PEACH, 0xbb8, 62, EVT_CMD(2, OP_SET), "Saffron: soup handed over (61) end"},
    {PEACH, 0xbd4, 62, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup handed over (62)"},
    {PEACH, 0xc04, 62, EVT_CMD(2, OP_SET), "Saffron: soup handed over (62) end"},
    {PEACH, 0xc20, 61, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup held (61)"},
    {PEACH, 0xc84, 62, EVT_CMD(2, OP_SET), "Saffron: soup held (61) end"},
    {PEACH, 0xca0, 62, EVT_CMD(2, OP_IF_EQUAL), "Saffron: soup held (62)"},
    {PEACH, 0xd04, 62, EVT_CMD(2, OP_SET), "Saffron: soup held (62) end"},
};

// ap_join_gift(pixlItemId): a Pixl join location. Marks it collected; until Archipelago is
// connected it still gives the vanilla Pixl.
static EVT_DEFINE_USER_FUNC(ap_join_gift)
{
    (void) isFirstCall;
    s32 item = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    spm::swdrv::swSet(AP_GSWF_JOIN(item));
    // Debug: act like a join holding another item, to test the rooms and deliveries without the Pixl:
    // per Pixl (joinItems), or for every join (debugFlags)
    mailbox::Mailbox & mb = mailbox::gMailbox;
    u16 given = mb.joinItems[item - ITEM_ID_FAIRY_START];
    if (given == JOIN_ITEM_OTHER_WORLD)
    {
        delivery::giveLocal(apitem::AP_ITEM_ID);
        wii::os::OSReport("[spmap] joins: Pixl %d's location holds another world's item\n", item);
    }
    else if (given != 0)
        delivery::giveLocal(given);
    else if (mb.debugFlags & DEBUG_JOINS_GIVE_NOTHING)
        ;
    else if (mb.debugFlags & DEBUG_JOINS_GIVE_PLACEHOLDER)
        delivery::giveLocal(mb.placeholderItem ? mb.placeholderItem : DEFAULT_PLACEHOLDER_ITEM);
    else
        spm::mario_pouch::pouchAddItem(item);
    mailbox::sendLocation(JOIN_LOCATION_BASE + item + (given == JOIN_ITEM_OTHER_WORLD ? JOIN_LOCATION_OTHER_WORLD : 0));
    wii::os::OSReport("[spmap] joins: location for Pixl %d collected\n", item);
    return 2;
}

// ap_skip(...): stands in for a skipped call
static EVT_DEFINE_USER_FUNC(ap_skip)
{
    (void) evt;
    (void) isFirstCall;
    return 2;
}

static bool havePixl(s32 pixl)
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->pixls)
    {
        if (info.itemType == pixl)
            return true;
    }
    return false;
}

// ap_join_select(pixlItemId): selects the Pixl only if Mario has it. Selecting one he doesn't have
// leaves no Pixl selected, and opening the Pixl menu then crashes
static EVT_DEFINE_USER_FUNC(ap_join_select)
{
    (void) isFirstCall;
    s32 item = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    if (havePixl(item))
        spm::mario_pouch::pouchSetPixlSelected(item);
    return 2;
}

// The calls in Fleep's join scene that only work if Mario really has him
static EVT_DEFINE_USER_FUNC(ap_fleep_mode_on)
{
    return havePixl(PIXL_FLEEP) ? gamesyms::evt_fleep_mode_on(evt, isFirstCall) : 2;
}
static EVT_DEFINE_USER_FUNC(ap_fleep_mode_set)
{
    return havePixl(PIXL_FLEEP) ? gamesyms::evt_fleep_mode_set(evt, isFirstCall) : 2;
}
static bool haveCharacter(s32 character)
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->characters)
    {
        if (info.itemType == CHARACTER_ITEM_BASE + character)
            return true;
    }
    return false;
}

// Gift location of each character's join scene, set up by init() from the CHAR_JOIN patches
static hubstory::Gift sCharacterGift[CHARACTER_LUIGI + 1];

// ap_char_join(character): a character's join scene. Collects its location (once), then switches
// to the character only if Mario has them: switching to one he doesn't have breaks his controls
static EVT_DEFINE_USER_FUNC(ap_char_join)
{
    s32 character = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    if (isFirstCall)
        hubstory::collectGift(sCharacterGift[character]);
    if (!haveCharacter(character))
    {
        wii::os::OSReport("[spmap] joins: character %d not in the party, staying as is\n", character);
        return 2;
    }
    return spm::evt_mario::evt_mario_set_character(evt, isFirstCall);
}

// ap_lesson(value): the end of Bestovius's lesson, where vanilla sets Chapter 1's value to 16
static EVT_DEFINE_USER_FUNC(ap_lesson)
{
    (void) isFirstCall;
    hubstory::onLesson(spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]));
    return 2;
}

// Peach's vanilla join scene (relF), set up by init() from the PEACH_JOIN patch
static const EvtScriptCode * sPeachScene;

// ap_peach_join(&scene): before Peach's join scene. Collects her location first, so a Peach placed
// there joins; with Peach, scene = the vanilla scene. Without her (it's played as Peach, with a Mario
// NPC beside her), scene = 0 and her chain just ends here
static EVT_DEFINE_USER_FUNC(ap_peach_join)
{
    (void) isFirstCall;
    hubstory::collectGift(hubstory::GIFT_PEACH);
    const EvtScriptCode * scene = haveCharacter(CHARACTER_PEACH) ? sPeachScene : nullptr;
    if (scene == nullptr)
    {
        savedata::setGswByte(AP_GSW_PEACH_CHAIN, PEACH_CHAIN_DONE);
        wii::os::OSReport("[spmap] joins: Peach not in the party, her join scene skipped\n");
    }
    spm::evtmgr_cmd::evtSetValue(evt, evt->pCurData[0], reinterpret_cast<s32>(scene));
    return 2;
}
static constexpr s32 ap_peach_join_parameter_count = 1;

EVT_BEGIN(ap_peach_join_evt)
    USER_FUNC(ap_peach_join, LW(1))
    IF_NOT_EQUAL(LW(1), 0)
        RUN_CHILD_EVT(LW(1))
    END_IF()
    RETURN()
EVT_END()

// Vanilla's soup talk script (relF), set up by init() from the PEACH_TALK patch
static const EvtScriptCode * sPeachSoupScene;

// ap_peach_soup(&scene): Mario talked to unconscious Peach (mac_01), which the map only allows while
// he has a Spicy Soup. With cutscenes on and Peach in the party, scene = vanilla's soup scene (played
// as Peach, as is the join scene it leads to). Otherwise scene = 0: the soup goes and her location is
// collected, its item given once Mario has control
static EVT_DEFINE_USER_FUNC(ap_peach_soup)
{
    (void) isFirstCall;
    const EvtScriptCode * scene = nullptr;
    if (mailbox::gMailbox.playCutscenes && haveCharacter(CHARACTER_PEACH))
        scene = sPeachSoupScene;
    else if (spm::mario_pouch::pouchCheckHaveItem(ITEM_SPICY_SOUP))
    {
        spm::mario_pouch::pouchRemoveItem(ITEM_SPICY_SOUP);
        hubstory::collectGift(hubstory::GIFT_PEACH);
        savedata::setGswByte(AP_GSW_PEACH_CHAIN, PEACH_CHAIN_DONE);
    }
    spm::evtmgr_cmd::evtSetValue(evt, evt->pCurData[0], reinterpret_cast<s32>(scene));
    return 2;
}
static constexpr s32 ap_peach_soup_parameter_count = 1;

// She leaves by moving away rather than being deleted: mac_01 keeps setting her talk script until
// the soup is gone
EVT_BEGIN(ap_peach_soup_evt)
    USER_FUNC(ap_peach_soup, LW(1))
    IF_NOT_EQUAL(LW(1), 0)
        RUN_CHILD_EVT(LW(1))
        RETURN()
    END_IF()
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    USER_FUNC(spm::evt_npc::evt_npc_set_position, PTR("peach"), 0, -1000, 0)
    USER_FUNC(spm::evt_mario::evt_mario_key_on)
    RETURN()
EVT_END()

using UserFunc = s32(spm::evtmgr::EvtEntry * evt, bool isFirstCall);
static const struct
{
    UserFunc * real;
    UserFunc * wrapper;
} sFleepOnly[] = {
    {gamesyms::evt_fleep_mode_on, ap_fleep_mode_on},
    {gamesyms::evt_fleep_mode_set, ap_fleep_mode_set},
};

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

static void apply(const Patch & p, EvtScriptCode * base)
{
    EvtScriptCode * at = base + p.offset / sizeof(EvtScriptCode);
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
            const s32 orig[] = {
                EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_pouch::evt_pouch_add_item), p.a,
                EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_pouch::evt_pouch_set_pixl_selected), p.a,
            };
            if (expect(at, orig, 6, p.what))
            {
                at[1] = reinterpret_cast<s32>(ap_join_gift);
                at[4] = reinterpret_cast<s32>(ap_join_select);
            }
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
        case FLEEP_ONLY:
        {
            const s32 orig[] = {EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(sFleepOnly[p.b].real), 1};
            if (expect(at, orig, 3, p.what))
                at[1] = reinterpret_cast<s32>(sFleepOnly[p.b].wrapper);
            break;
        }
        case CHAR_JOIN:
        {
            const s32 orig[] = {EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_mario::evt_mario_set_character), p.a};
            if (expect(at, orig, 3, p.what))
            {
                sCharacterGift[p.a] = static_cast<hubstory::Gift>(p.b);
                at[1] = reinterpret_cast<s32>(ap_char_join);
            }
            break;
        }
        case GIFT_GATE:
        {
            const s32 orig[] = {EVT_CMD(2, OP_IF_LARGE_EQUAL), GSW_VAR(0), p.a};
            if (expect(at, orig, 3, p.what))
            {
                at[0] = EVT_CMD(2, OP_IF_EQUAL);
                at[1] = GSWF_VAR(AP_GSWF_GIFT(p.b));
                at[2] = 0;
            }
            break;
        }
        case ALWAYS:
        {
            const s32 orig[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), p.a};
            if (expect(at, orig, 3, p.what))
                at[2] = STORY_VALUE_NEVER;
            break;
        }
        case CASE_GSW:
        {
            const s32 orig[] = {EVT_CMD(1, OP_CASE_SMALL), p.a};
            if (expect(at, orig, 2, p.what))
                at[1] = GSW_VAR(p.b);
            break;
        }
        case LESSON_IF:
        {
            const s32 orig[] = {EVT_CMD(2, p.b), GSW_VAR(0), p.a};
            if (expect(at, orig, 3, p.what))
                at[1] = GSW_VAR(AP_GSW_LESSON);
            break;
        }
        case LESSON:
        {
            const s32 orig[] = {EVT_CMD(2, OP_SET), GSW_VAR(0), p.a};
            if (expect(at, orig, 3, p.what))
            {
                at[0] = EVT_CMD(2, OP_USER_FUNC);
                at[1] = reinterpret_cast<s32>(ap_lesson);
            }
            break;
        }
        case PEACH_JOIN:
        {
            const s32 orig[] = {EVT_CMD(1, OP_RUN_EVT), reinterpret_cast<s32>(gamesyms::relF_section5 + p.a / sizeof(EvtScriptCode))};
            if (expect(at, orig, 2, p.what))
            {
                sPeachScene = reinterpret_cast<const EvtScriptCode *>(at[1]);
                at[1] = reinterpret_cast<s32>(ap_peach_join_evt);
            }
            break;
        }
        case PEACH_TALK:
        {
            const s32 orig[] = {
                EVT_CMD(4, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_npc::evt_npc_set_property), at[2], NPC_PROPERTY_TALK,
                reinterpret_cast<s32>(gamesyms::relF_section5 + p.a / sizeof(EvtScriptCode)),
            };
            if (expect(at, orig, 5, p.what))
            {
                sPeachSoupScene = reinterpret_cast<const EvtScriptCode *>(at[4]);
                at[4] = reinterpret_cast<s32>(ap_peach_soup_evt);
            }
            break;
        }
        case PEACH:
        {
            const s32 orig[] = {p.b, GSW_VAR(0), p.a};
            if (expect(at, orig, p.b == EVT_CMD(1, OP_SWITCH) ? 2 : 3, p.what))
                at[1] = GSW_VAR(AP_GSW_PEACH_CHAIN);
            break;
        }
        case SKIP:
        {
            const s32 orig[] = {EVT_CMD(p.a + 1, OP_USER_FUNC), reinterpret_cast<s32>(sSkippable[p.b])};
            if (expect(at, orig, 2, p.what))
                at[1] = reinterpret_cast<s32>(ap_skip);
            break;
        }
    }
}

void init()
{
    for (auto & p : sPatches)
        apply(p, gamesyms::relF_section5);
    EvtScriptCode * saffron = const_cast<EvtScriptCode *>(spm::evt_shop::saffronCookingScript);
    for (auto & p : sSaffronPatches)
        apply(p, saffron);
    wii::os::OSReport("[spmap] joins: patched\n");
}

// A save can have Pixls but none selected (from a join before ap_join_select existed, or a Pixl
// received from the client, which pouchAddItem doesn't select): select one
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

// Watchitt's patched CASE_SMALL: below this value he says "go get help"
#define WATCHITT_WAIT 26  // past 25, his "send for the builder" case
#define WATCHITT_NEVER 0  // below every Chapter 1 value

void tick()
{
    savedata::setGswByte(AP_GSW_WATCHITT, havePixl(PIXL_THOREAU) ? WATCHITT_NEVER : WATCHITT_WAIT);
}

void onMapChange()
{
    repairPixlSelection();

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
