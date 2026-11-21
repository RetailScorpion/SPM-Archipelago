#include <common.h>
#include <msl/string.h>
#include <spm/npcdrv.h>
#include <wii/os.h>

#include "boss_fall_guard.h"

namespace mod::boss_fall_guard {

#define NPC_ACTIVE 0x1        // flag8
#define OCHUNKS_NAME "dodon"  // O'Chunks' NPC name in his fights
#define FALL_LIMIT 600.0f     // this far below where he last stood counts as fallen out
#define TELEPORT 150.0f       // moved this far in one frame: put there by a script, not falling
#define MAX_TRACKED 80

// Where each NPC slot's O'Chunks last stood, whether that's known, and his height last frame
static wii::mtx::Vec3 sLastGood[MAX_TRACKED];
static bool sKnown[MAX_TRACKED];
static f32 sLastY[MAX_TRACKED];

void onMapChange()
{
    for (bool & known : sKnown)
        known = false;
}

void tick()
{
    spm::npcdrv::NPCWork * work = spm::npcdrv::npcGetWorkPtr();
    s32 count = work->num < MAX_TRACKED ? work->num : MAX_TRACKED;
    for (s32 slot = 0; slot < count; slot++)
    {
        spm::npcdrv::NPCEntry & npc = work->entries[slot];
        if (!(npc.flag8 & NPC_ACTIVE) || msl::string::strcmp(npc.name, OCHUNKS_NAME) != 0)
        {
            sKnown[slot] = false;
            sLastY[slot] = 0;
            continue;
        }
        wii::mtx::Vec3 & at = npc.position;
        // A script moving him (scenes hide the cutscene O'Chunks at height -1000 while the fight's
        // one is out; user, 2026-11-16: caught as a fall, that left two on screen) isn't a fall:
        // forget where he stood until he's steady again
        f32 step = at.y - sLastY[slot];
        sLastY[slot] = at.y;
        if (step > TELEPORT || step < -TELEPORT)
        {
            sKnown[slot] = false;
            continue;
        }
        if (sKnown[slot] && at.y < sLastGood[slot].y - FALL_LIMIT)
        {
            wii::os::OSReport("[spmap] fallguard: O'Chunks fell out (y %d), put back at y %d\n",
                              static_cast<s32>(at.y), static_cast<s32>(sLastGood[slot].y));
            at = sLastGood[slot];
            continue;
        }
        // Remember only steady ground: not mid-fall (a hop's peak is fine; it's above)
        if (!sKnown[slot] || at.y >= sLastGood[slot].y - 1.0f)
        {
            sLastGood[slot] = at;
            sKnown[slot] = true;
        }
    }
}

}
