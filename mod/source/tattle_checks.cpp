#include <common.h>
#include <msl/string.h>
#include <evt_cmd.h>
#include <spm/evt_mario.h>
#include <spm/evt_msg.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/fairy.h>
#include <spm/item_data.h>
#include <spm/mario.h>
#include <spm/mario_pouch.h>
#include <spm/npcdrv.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "game_mode.h"
#include "code_patching.h"
#include "game_addresses.h"
#include "location_flags.h"
#include "mailbox.h"
#include "save_data.h"
#include "tattle_checks.h"

namespace mod::tattle_checks {

#define TATL 220  // ITEM_ID_FAIRY_MONOSHIRI: the unused Tippi Pixl
#define TATL_NAME "ap_tatl_name"
#define TATL_DESC "ap_tatl_desc"
#define TATL_TATTLE "ap_tatl_tattle"  // the tattle Tatl shows (text in sTattleText)

#define FLEEP 226                      // Fleep's Pixl id, in his motion
#define LI_R3_FLEEP 0x386000E2         // li r3, 0xe2
#define MOT_FLEEP 0x26
#define FLEEP_STAGE_USE 30             // his motion's stage after 1 is pressed again (the flip)
#define FLEEP_STAGE_END 40             // ... and the stage that puts the Pixl away (also B's)
#define NPC_ACTIVE 0x1                 // NPCEntry flag8
#define NPC_FREEZE_ALL 0x1             // NPCWork flag

// How near Tatl an NPC has to be, in world units (in 2D, where Fleep's motion runs): across, and
// below and above its feet
#define REACH_X 32.0f
#define REACH_BELOW 24.0f
#define REACH_ABOVE 72.0f

static char sTattleText[0x440];

// Whether this seed has tattle checks (and the seed's items are in the game).
static bool enabled()
{
    return mailbox::gMailbox.seedPlaced && (mailbox::gMailbox.seedFlags & SEED_FLAG_TATTLE_CHECKS);
}

// The kind of enemy (its position in TATTLE_CARDS) whose catch card this is, or -1.
static s32 kindOfCard(s32 card)
{
    for (s32 k = 0; k < location_flags::TATTLE_KIND_COUNT; k++)
        if (location_flags::TATTLE_CARDS[k] == card)
            return k;
    return -1;
}

static const char * (*searchGetNpcMsgReal)(spm::npcdrv::NPCEntry * npc);
static const char * searchGetNpcMsgHook(spm::npcdrv::NPCEntry * npc)
{
    const char * text = searchGetNpcMsgReal(npc);
    if (!enabled() || spm::mario_pouch::pouchGetCurPixl() != TATL)
        return text;
    s32 kind = kindOfCard(spm::npcdrv::npcTribes[npc->tribeId].catchCardItemId);
    if (kind < 0)
        return text;
    s32 flag = AP_GSWF_TATTLE_BASE + kind;
    if (!spm::swdrv::swGet(flag))
    {
        spm::swdrv::swSet(flag);
        wii::os::OSReport("[spmap] tattle: tribe %d (kind %d) tattled with Tatl\n", npc->tribeId, kind);
    }
    return text;
}

// ---- Tatl's pointer: Fleep's motion, with Tatl as the Pixl ----

// Pressing 1 with Tatl: Fleep's pointer (moved with the arrow buttons; 1 again tattles, B cancels).
// The game's entry for this Pixl is an assert.
static s32 tatlAbility(s32, s32 mode)
{
    if (game_addresses::fairy_work_by_id(TATL) == nullptr)
        return 0;  // no Tatl out to move around
    return game_addresses::fleep_ability(FLEEP, mode);
}

// Replaces "li r3, 0xe2" in Fleep's motion: the Pixl it moves around is Tatl when she's the one out.
static s32 fleepMotionPixl()
{
    return spm::mario_pouch::pouchGetCurPixl() == TATL ? TATL : FLEEP;
}

// Fleep's motion plays animations Tatl's model doesn't have (A_1, A_2): she uses her primary one
// (user 2026-11-07: until her custom model, which will have two)
static void (*fairyChangeAnimReal)(void * fairy, const char * anim);
static void fairyChangeAnimHook(void * fairy, const char * anim)
{
    auto * entry = static_cast<spm::fairy::FairyEntry *>(fairy);
    if (entry->itemId == TATL && anim != nullptr && anim[0] == 'A' && anim[1] == '_')
        anim = "S_1";
    fairyChangeAnimReal(fairy, anim);
}

// ap_tatl_freeze(on): stops (or restarts) every NPC while her tattle is up
static EVT_DEFINE_USER_FUNC(ap_tatl_freeze)
{
    (void) isFirstCall;
    spm::npcdrv::NPCWork * work = spm::npcdrv::npcGetWorkPtr();
    if (spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]))
        work->flag |= NPC_FREEZE_ALL;
    else
        work->flag &= ~NPC_FREEZE_ALL;
    return 2;
}
static constexpr s32 ap_tatl_freeze_parameter_count = 1;

EVT_BEGIN(ap_tatl_tattle_evt)
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    USER_FUNC(ap_tatl_freeze, 1)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(TATL_TATTLE), 0, 0)
    USER_FUNC(ap_tatl_freeze, 0)
    USER_FUNC(spm::evt_mario::evt_mario_key_on)
    RETURN()
EVT_END()

// The NPC nearest Tatl's point, within reach, or nullptr.
static spm::npcdrv::NPCEntry * npcAtTatl(const wii::mtx::Vec3 & at)
{
    spm::npcdrv::NPCWork * work = spm::npcdrv::npcGetWorkPtr();
    spm::npcdrv::NPCEntry * best = nullptr;
    f32 bestDistance = 0.0f;
    for (s32 i = 0; i < work->num; i++)
    {
        spm::npcdrv::NPCEntry * npc = &work->entries[i];
        if (!(npc->flag8 & NPC_ACTIVE) || npc->master != nullptr)
            continue;
        f32 dx = at.x - npc->position.x;
        f32 dy = at.y - npc->position.y;
        if (dx < -REACH_X || dx > REACH_X || dy < -REACH_BELOW || dy > REACH_ABOVE)
            continue;
        f32 dyMiddle = dy - REACH_ABOVE / 3.0f;
        f32 distance = dx * dx + dyMiddle * dyMiddle;
        if (best == nullptr || distance < bestDistance)
        {
            best = npc;
            bestDistance = distance;
        }
    }
    return best;
}

void tick()
{
    spm::mario::MarioWork * mario = spm::mario::marioGetPtr();
    if (mario->motionId != MOT_FLEEP || mario->subMotionId != FLEEP_STAGE_USE
        || spm::mario_pouch::pouchGetCurPixl() != TATL)
        return;
    mario->subMotionId = FLEEP_STAGE_END;  // no flip: straight to putting her away
    auto * tatl = static_cast<spm::fairy::FairyEntry *>(game_addresses::fairy_work_by_id(TATL));
    if (tatl == nullptr)
        return;
    spm::npcdrv::NPCEntry * npc = npcAtTatl(tatl->position);
    if (npc == nullptr)
    {
        wii::os::OSReport("[spmap] tattle: nothing at Tatl (%d %d)\n", static_cast<s32>(tatl->position.x),
                          static_cast<s32>(tatl->position.y));
        return;
    }
    // Her text: Tippi's tattle (which also sends the check, above), without its search-window tag
    const char * text = game_addresses::searchGetNpcMsg(npc);
    if (text == nullptr)
        return;
    const char * tag = "<gsearch>\n";
    u32 tagLength = msl::string::strlen(tag);
    if (msl::string::strncmp(text, tag, tagLength) == 0)
        text += tagLength;
    msl::string::strncpy(sTattleText, text, sizeof(sTattleText) - 1);
    sTattleText[sizeof(sTattleText) - 1] = '\0';
    spm::evtmgr::evtEntry(ap_tatl_tattle_evt, 0, 0);
    wii::os::OSReport("[spmap] tattle: Tatl tattled %s (tribe %d)\n", npc->name, npc->tribeId);
}

void init()
{
    spm::item_data::ItemData & tatl = spm::item_data::itemDataTable[TATL];
    game_mode::set(&tatl.nameMsg, static_cast<const char *>(TATL_NAME));
    game_mode::set(&tatl.descMsg, static_cast<const char *>(TATL_DESC));
    for (auto * entry = game_addresses::pixl_ability_table; entry->pixl != 0; entry++)
        if (entry->pixl == TATL)
            game_mode::set(&entry->use, &tatlAbility);
    searchGetNpcMsgReal = game_mode::hook(game_addresses::searchGetNpcMsg, searchGetNpcMsgHook);
    fairyChangeAnimReal = game_mode::hook(game_addresses::fairy_change_anim, fairyChangeAnimHook);
    u32 * loads[] = {game_addresses::fleep_motion_pixl_id_load, game_addresses::fleep_end_pixl_id_load};
    for (u32 * at : loads)
    {
        if (*at != LI_R3_FLEEP)
        {
            wii::os::OSReport("[spmap] tattle: Fleep's motion isn't what was expected (%08x)\n", *at);
            continue;
        }
        game_mode::writeBranch(at, reinterpret_cast<void *>(fleepMotionPixl), true);
    }
    wii::os::OSReport("[spmap] tattle: Tatl set up\n");
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, TATL_NAME) == 0)
        return "Tatl";
    if (msl::string::strcmp(msgName, TATL_DESC) == 0)
        return "Press 1 and move Tatl with the arrow buttons.\nPress 1 again over an enemy to learn about it.";
    if (msl::string::strcmp(msgName, TATL_TATTLE) == 0)
        return sTattleText;
    return nullptr;
}

}
