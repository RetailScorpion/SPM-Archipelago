#include <common.h>
#include <msl/string.h>
#include <spm/npcdrv.h>
#include <spm/spmario.h>

#include "options_menu.h"
#include "sammer_ninjas.h"

namespace mod::sammer_ninjas {

#define NPC_HIDDEN_FROZEN 0x20000  // npcdrv.h flag46C
#define NPC_ACTIVE 0x1             // flag8
#define NINJA_TRIBE_FIRST 211      // Ninjoe (e_ninja); 212-222: the other ninjas, their parts and bombs
#define NINJA_TRIBE_LAST 222
#define SAMMER_MAP_PREFIX "wa"

#define MAX_TRACKED 256

// Which NPC slots the mod hid, with the NPC's id then (a slot may be reused by another NPC)
static bool sHidden[MAX_TRACKED];
static s32 sHiddenId[MAX_TRACKED];
static s32 sHiddenCount;

static bool isNinja(const spm::npcdrv::NPCEntry & npc)
{
    return (npc.flag8 & NPC_ACTIVE) && npc.tribeId >= NINJA_TRIBE_FIRST && npc.tribeId <= NINJA_TRIBE_LAST;
}

void tick()
{
    spm::npcdrv::NPCWork * work = spm::npcdrv::npcGetWorkPtr();
    if (work == nullptr || work->entries == nullptr)
        return;
    bool on = options_menu::get(options_menu::OPTION_NO_SAMMER_NINJAS) &&
              msl::string::strncmp(spm::spmario::gp->mapName, SAMMER_MAP_PREFIX, 2) == 0;
    if (!on && sHiddenCount == 0)
        return;
    s32 count = work->num < MAX_TRACKED ? work->num : MAX_TRACKED;
    for (s32 slot = 0; slot < count; slot++)
    {
        spm::npcdrv::NPCEntry & npc = work->entries[slot];
        if (sHidden[slot])
        {
            // The setting is off again, or the slot holds another NPC now
            if (!on || sHiddenId[slot] != npc.id || !isNinja(npc))
            {
                if (sHiddenId[slot] == npc.id)
                    npc.flag46C &= ~NPC_HIDDEN_FROZEN;
                sHidden[slot] = false;
                sHiddenCount--;
            }
            continue;
        }
        if (!on || !isNinja(npc) || (npc.flag46C & NPC_HIDDEN_FROZEN))
            continue;  // (already hidden or frozen by the game: left alone)
        npc.flag46C |= NPC_HIDDEN_FROZEN;
        sHidden[slot] = true;
        sHiddenId[slot] = npc.id;
        sHiddenCount++;
    }
}

void onMapChange()
{
    for (s32 slot = 0; slot < MAX_TRACKED; slot++)
        sHidden[slot] = false;
    sHiddenCount = 0;
}

}
