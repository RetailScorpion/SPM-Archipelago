#include <common.h>
#include <spm/mario.h>
#include <spm/npcdrv.h>
#include <wii/os.h>

#include "npc_culling.h"
#include "options_menu.h"

namespace mod::npc_culling {

// The game's "hidden and frozen" NPC flag (npcdrv.h flag46C)
#define NPC_HIDDEN_FROZEN 0x20000
#define NPC_ACTIVE 0x1  // flag8

// Off screen (game units from Mario): culled beyond these, brought back inside the smaller ones
#define CULL_ACROSS 900.0f
#define CULL_UPDOWN 700.0f
#define BACK_ACROSS 800.0f
#define BACK_UPDOWN 600.0f

#define MAX_TRACKED 256

// Which NPC slots the mod culled, with the NPC's id then (a slot may be reused by another NPC)
static bool sCulled[MAX_TRACKED];
static s32 sCulledId[MAX_TRACKED];
static s32 sCulledCount;

static f32 absf(f32 v)
{
    return v < 0 ? -v : v;
}

// Whether an NPC is one the mod may cull: an enemy the room's setup file placed, on its own.
static bool cullable(const spm::npcdrv::NPCEntry & npc)
{
    return (npc.flag8 & NPC_ACTIVE) && npc.setupFileIndex > 0 && npc.master == nullptr && npc.hp > 0;
}

static void bringBack(spm::npcdrv::NPCEntry & npc, s32 slot)
{
    if (sCulledId[slot] == npc.id)
        npc.flag46C &= ~NPC_HIDDEN_FROZEN;
    sCulled[slot] = false;
    sCulledCount--;
}

void tick()
{
    spm::npcdrv::NPCWork * work = spm::npcdrv::npcGetWorkPtr();
    if (work == nullptr || work->entries == nullptr)
        return;
    s32 count = work->num < MAX_TRACKED ? work->num : MAX_TRACKED;
    bool on = options_menu::get(options_menu::OPTION_CULL_ENEMIES);
    if (!on && sCulledCount == 0)
        return;

    const wii::mtx::Vec3 & mario = spm::mario::marioGetPtr()->position;
    for (s32 slot = 0; slot < count; slot++)
    {
        spm::npcdrv::NPCEntry & npc = work->entries[slot];
        if (sCulled[slot])
        {
            // Back on, or no longer an enemy the mod may cull (gone, defeated, another NPC in the slot)
            f32 across = absf(npc.position.x - mario.x);
            f32 depth = absf(npc.position.z - mario.z);
            if (depth > across)
                across = depth;
            f32 updown = absf(npc.position.y - mario.y);
            bool near = across < BACK_ACROSS && updown < BACK_UPDOWN;
            if (!on || near || !cullable(npc) || sCulledId[slot] != npc.id)
                bringBack(npc, slot);
            continue;
        }
        if (!on || !cullable(npc) || (npc.flag46C & NPC_HIDDEN_FROZEN))
            continue;  // (already hidden or frozen by the game: not the mod's to touch)
        f32 across = absf(npc.position.x - mario.x);
        f32 depth = absf(npc.position.z - mario.z);
        if (depth > across)
            across = depth;
        f32 updown = absf(npc.position.y - mario.y);
        if (across > CULL_ACROSS || updown > CULL_UPDOWN)
        {
            npc.flag46C |= NPC_HIDDEN_FROZEN;
            sCulled[slot] = true;
            sCulledId[slot] = npc.id;
            sCulledCount++;
        }
    }
}

void onMapChange()
{
    // The room's NPCs are about to be replaced: nothing of the mod's is left to clear
    for (s32 slot = 0; slot < MAX_TRACKED; slot++)
        sCulled[slot] = false;
    sCulledCount = 0;
}

}
