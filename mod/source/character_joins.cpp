#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_mario.h>
#include <spm/evt_npc.h>
#include <spm/evt_shop.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario_pouch.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "character_joins.h"
#include "game_addresses.h"
#include "mailbox.h"
#include "save_data.h"
#include "script_patching.h"
#include "story_gifts.h"
#include "story_progress.h"

namespace mod::character_joins {

using namespace mod::script_patching;

// Characters, as the game's evt_mario_set_character takes them; as items they're 216 + character
#define CHARACTER_PEACH 1
#define CHARACTER_BOWSER 2
#define CHARACTER_LUIGI 3
#define CHARACTER_ITEM_BASE 216

#define ITEM_SPICY_SOUP 165
#define NPC_PROPERTY_TALK 9  // evt_npc_set_property: the script that runs when Mario talks to the NPC

// A story value past every real one, for patches that should always pass
#define STORY_VALUE_NEVER 30000

// AP_GSW_PEACH_SCENE when Peach's join scene is due: past the hub's fixed value, so the patched
// "story value smaller than" case passes. 0 (never) otherwise
#define PEACH_SCENE_ON (STORY_HUB_VALUE + 1)

// ---- The patches ----

enum PatchKind
{
    CHAR_JOIN,   // the "switch to character a" call -> ap_char_join: collects gift location b, then
                 // switches only if Mario has the character
    GIFT_GATE,   // IF_LARGE_EQUAL(story value, a) -> IF_EQUAL(gift location b's flag, 0)
    ALWAYS,      // IF_SMALL(story value, a) -> IF_SMALL(story value, STORY_VALUE_NEVER): always true
    CASE_GSW,    // CASE_SMALL(a) -> CASE_SMALL(save byte b)
    SKIP,        // a call with a arguments to function b (SkippableCall) -> does nothing
    PEACH,       // command b on the story value (with value a, unless b is a SWITCH) -> the same
                 // command on Peach's chain value instead
    PEACH_JOIN,  // RUN_EVT(Peach's join scene, at relF offset a) -> RUN_EVT(ap_peach_join_evt)
    PEACH_TALK,  // Peach's talk script (at relF offset a) -> ap_peach_soup_evt
};

struct Patch
{
    PatchKind kind;
    u32 offset;  // bytes from the start of relF's script section, or of Saffron's script
    s32 a, b;
    const char * what;
};

// Each one was checked against the game's original words (see DESIGN.md)
static const Patch sPatches[] = {
    // Peach in Flipside (mac_01): unconscious with Merlon while the chain is 60-62
    {PEACH, 0x02f8c, 60, EVT_CMD(2, OP_IF_LARGE_EQUAL), "Peach: mac_01 Peach and Merlon (from)"},
    {PEACH, 0x02f98, 63, EVT_CMD(2, OP_IF_SMALL), "Peach: mac_01 Peach and Merlon (to)"},
    {PEACH, 0x0317c, 60, EVT_CMD(2, OP_IF_LARGE), "Peach: mac_01 Merlon's spot"},
    {PEACH, 0x036e4, 0, EVT_CMD(1, OP_SWITCH), "Peach: mac_01 unconscious scene trigger"},
    {PEACH, 0x02908, 61, EVT_CMD(2, OP_SET), "Peach: mac_01 unconscious scene end"},
    {PEACH_TALK, 0x03168, 0x00230, 0, "Peach: talk with the Spicy Soup"},
    // (Giving the soup at 61, before Saffron, walks a townsperson over who never arrives: that check
    // keeps reading the hub value, so it takes the "after Saffron" branch)

    // Peach's join scene in Flipside Tower (mac_02), entered from Merlon's house
    {PEACH, 0x19c78, 61, EVT_CMD(2, OP_IF_LARGE_EQUAL), "Peach: mac_02 scene actors (from)"},
    {PEACH, 0x19c84, 63, EVT_CMD(2, OP_IF_SMALL), "Peach: mac_02 scene actors (to)"},
    {PEACH, 0x0d4ec, 63, EVT_CMD(2, OP_SET), "Peach: join scene end"},
    // The scene's SWITCHes whose other cases the hub value never reaches: who counts as inside
    // Merlon's house (61-62 adds the Mario NPC the scene waits for), its music and its sound
    {PEACH, 0x1aee0, 0, EVT_CMD(1, OP_SWITCH), "Peach: Merlon's house NPCs"},
    {PEACH, 0x1bab4, 0, EVT_CMD(1, OP_SWITCH), "Peach: scene music"},
    {PEACH, 0x1bb6c, 0, EVT_CMD(1, OP_SWITCH), "Peach: scene sound"},
    {PEACH_JOIN, 0x1bdf4, 0xca08, 0, "Peach: join scene without Peach"},
    // mac_02 runs the join scene for CASE_SMALL(63) of its story value SWITCH; the case now compares
    // with a save byte onMapChange sets once the chain reaches 61-62
    {CASE_GSW, 0x1bdd4, 63, AP_GSW_PEACH_SCENE, "Peach: join scene trigger"},
    {SKIP, 0xcaf8, 1, CALL_ADD_ITEM, "Peach: vanilla join"},
    {CHAR_JOIN, 0xcb04, CHARACTER_PEACH, story_gifts::GIFT_PEACH, "Peach join"},

    // Bowser: 3-1 (ta1_04), after his fight
    {SKIP, 0x74744, 1, CALL_ADD_ITEM, "Bowser: vanilla join"},
    {CHAR_JOIN, 0x74724, CHARACTER_BOWSER, story_gifts::GIFT_BOWSER, "Bowser join"},

    // Luigi: the Underwhere (an1_07) plays his scene for story values 295-296; now until collected
    {GIFT_GATE, 0xede0c, 295, story_gifts::GIFT_LUIGI, "Luigi: join scene"},
    {ALWAYS, 0xede18, 297, 0, "Luigi: join scene (upper bound)"},
    {SKIP, 0xeda4c, 1, CALL_ADD_ITEM, "Luigi: vanilla join"},
    {CHAR_JOIN, 0xeda20, CHARACTER_LUIGI, story_gifts::GIFT_LUIGI, "Luigi join"},
};

// Saffron's cooking script (in the main program), offsets from saffronCookingScript: her Spicy
// Soup for Peach (story values 61-62) follows Peach's chain value
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

// ---- Script functions the patched scenes call ----
// (Each returns 2, which tells the script engine the function has finished.)

// Whether this character is in Mario's party.
static bool haveCharacter(s32 character)
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->characters)
    {
        if (info.itemType == CHARACTER_ITEM_BASE + character)
            return true;
    }
    return false;
}

// Which gift location each character's join scene collects (filled in from the CHAR_JOIN patches)
static story_gifts::Gift sCharacterGift[CHARACTER_LUIGI + 1];

// ap_char_join(character): a character's join scene. Collects its location (once), then switches
// to the character only if Mario has them.
static EVT_DEFINE_USER_FUNC(ap_char_join)
{
    s32 character = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    if (isFirstCall)
        story_gifts::collectGift(sCharacterGift[character]);
    if (!haveCharacter(character))
    {
        wii::os::OSReport("[spmap] joins: character %d not in the party, staying as is\n", character);
        return 2;
    }
    return spm::evt_mario::evt_mario_set_character(evt, isFirstCall);
}

// Peach's vanilla join scene (found by the PEACH_JOIN patch)
static const EvtScriptCode * sPeachJoinScene;

// ap_peach_join(&scene): just before Peach's join scene. Collects her location first, so a Peach
// placed there can join. With Peach in the party, scene = the vanilla scene. Without her (it's
// played as Peach, with a Mario NPC beside her), scene = 0 and her chain simply ends.
static EVT_DEFINE_USER_FUNC(ap_peach_join)
{
    (void) isFirstCall;
    story_gifts::collectGift(story_gifts::GIFT_PEACH);
    const EvtScriptCode * scene = haveCharacter(CHARACTER_PEACH) ? sPeachJoinScene : nullptr;
    if (scene == nullptr)
    {
        save_data::setGswByte(AP_GSW_PEACH_CHAIN, PEACH_CHAIN_DONE);
        wii::os::OSReport("[spmap] joins: Peach not in the party, her join scene skipped\n");
    }
    spm::evtmgr_cmd::evtSetValue(evt, evt->pCurData[0], reinterpret_cast<s32>(scene));
    return 2;
}
static constexpr s32 ap_peach_join_parameter_count = 1;

// Runs instead of Peach's join scene: plays it only if ap_peach_join says so.
EVT_BEGIN(ap_peach_join_evt)
    USER_FUNC(ap_peach_join, LW(1))
    IF_NOT_EQUAL(LW(1), 0)
        RUN_CHILD_EVT(LW(1))
    END_IF()
    RETURN()
EVT_END()

// Vanilla's "give Peach the soup" talk script (found by the PEACH_TALK patch)
static const EvtScriptCode * sPeachSoupScene;

// ap_peach_soup(&scene): Mario talked to unconscious Peach in Flipside, which the map only allows
// while he has a Spicy Soup. With cutscenes on and Peach in the party, scene = vanilla's soup scene
// (played as Peach, like the join scene it leads to). Otherwise scene = 0: the soup is used up and
// her location collected; its item is given once Mario has control.
static EVT_DEFINE_USER_FUNC(ap_peach_soup)
{
    (void) isFirstCall;
    const EvtScriptCode * scene = nullptr;
    if (mailbox::gMailbox.playCutscenes && haveCharacter(CHARACTER_PEACH))
        scene = sPeachSoupScene;
    else if (spm::mario_pouch::pouchCheckHaveItem(ITEM_SPICY_SOUP))
    {
        spm::mario_pouch::pouchRemoveItem(ITEM_SPICY_SOUP);
        story_gifts::collectGift(story_gifts::GIFT_PEACH);
        save_data::setGswByte(AP_GSW_PEACH_CHAIN, PEACH_CHAIN_DONE);
    }
    spm::evtmgr_cmd::evtSetValue(evt, evt->pCurData[0], reinterpret_cast<s32>(scene));
    return 2;
}
static constexpr s32 ap_peach_soup_parameter_count = 1;

// Runs when Mario talks to unconscious Peach. Without the vanilla scene, she leaves by being moved
// far away rather than deleted, since the map keeps setting her talk script until the soup is gone.
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

// ---- Applying the patches ----

// The vanilla script at relF offset `offset`, as a script command would refer to it.
static s32 relfScriptReference(u32 offset)
{
    return reinterpret_cast<s32>(relfScript(offset));
}

// Applies one patch at `at` (after checking the words it changes are what's expected).
static void applyPatch(const Patch & p, EvtScriptCode * at)
{
    switch (p.kind)
    {
        case CHAR_JOIN:
        {
            const s32 expected[] = {EVT_CMD(2, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_mario::evt_mario_set_character), p.a};
            if (matches(at, expected, 3, p.what))
            {
                sCharacterGift[p.a] = static_cast<story_gifts::Gift>(p.b);
                at[1] = reinterpret_cast<s32>(ap_char_join);
            }
            break;
        }
        case GIFT_GATE:
        {
            const s32 expected[] = {EVT_CMD(2, OP_IF_LARGE_EQUAL), GSW_VAR(0), p.a};
            if (matches(at, expected, 3, p.what))
            {
                at[0] = EVT_CMD(2, OP_IF_EQUAL);
                at[1] = GSWF_VAR(AP_GSWF_GIFT(p.b));
                at[2] = 0;
            }
            break;
        }
        case ALWAYS:
        {
            const s32 expected[] = {EVT_CMD(2, OP_IF_SMALL), GSW_VAR(0), p.a};
            if (matches(at, expected, 3, p.what))
                at[2] = STORY_VALUE_NEVER;
            break;
        }
        case CASE_GSW:
            caseComparesWithSaveByte(at, p.a, p.b, p.what);
            break;
        case SKIP:
            skipCall(at, p.a, static_cast<SkippableCall>(p.b), p.what);
            break;
        case PEACH:
        {
            // A SWITCH has no value word to check, only the command and the story value
            const s32 expected[] = {p.b, GSW_VAR(0), p.a};
            if (matches(at, expected, p.b == EVT_CMD(1, OP_SWITCH) ? 2 : 3, p.what))
                at[1] = GSW_VAR(AP_GSW_PEACH_CHAIN);
            break;
        }
        case PEACH_JOIN:
        {
            const s32 expected[] = {EVT_CMD(1, OP_RUN_EVT), relfScriptReference(p.a)};
            if (matches(at, expected, 2, p.what))
            {
                sPeachJoinScene = reinterpret_cast<const EvtScriptCode *>(at[1]);
                at[1] = reinterpret_cast<s32>(ap_peach_join_evt);
            }
            break;
        }
        case PEACH_TALK:
        {
            // evt_npc_set_property(npc, talk, script); the NPC (word 2) can be anything
            const s32 expected[] = {
                EVT_CMD(4, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_npc::evt_npc_set_property), at[2], NPC_PROPERTY_TALK,
                relfScriptReference(p.a),
            };
            if (matches(at, expected, 5, p.what))
            {
                sPeachSoupScene = reinterpret_cast<const EvtScriptCode *>(at[4]);
                at[4] = reinterpret_cast<s32>(ap_peach_soup_evt);
            }
            break;
        }
    }
}

void init()
{
    for (auto & p : sPatches)
        applyPatch(p, relfScript(p.offset));
    EvtScriptCode * saffron = const_cast<EvtScriptCode *>(spm::evt_shop::saffronCookingScript);
    for (auto & p : sSaffronPatches)
        applyPatch(p, saffron + p.offset / sizeof(EvtScriptCode));
    wii::os::OSReport("[spmap] joins: character joins patched\n");
}

// ---- Every map change ----

void onMapChange()
{
    // Peach's chain value: done once her location is collected; otherwise back to the start if it
    // holds anything outside 60-62 (a save from before the chain had its own value)
    s32 chain = save_data::getGswByte(AP_GSW_PEACH_CHAIN);
    if (spm::swdrv::swGet(AP_GSWF_GIFT(story_gifts::GIFT_PEACH)))
        chain = PEACH_CHAIN_DONE;
    else if (chain < PEACH_CHAIN_START || chain >= PEACH_CHAIN_DONE)
        chain = PEACH_CHAIN_START;
    save_data::setGswByte(AP_GSW_PEACH_CHAIN, chain);

    // The join scene is due once she can be given the soup, as in vanilla (61-62). A Spicy Soup
    // from anywhere works
    bool sceneDue = chain > PEACH_CHAIN_START && chain < PEACH_CHAIN_DONE;
    save_data::setGswByte(AP_GSW_PEACH_SCENE, sceneDue ? PEACH_SCENE_ON : 0);
}

}
