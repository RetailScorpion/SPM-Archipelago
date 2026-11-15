#pragma once

#include <common.h>

/*
    "No ninjas in Sammer duels" (Options setting, on by default since 2026-11-14; user, 2026-11-08). In Sammer's Kingdom
    (wa* maps), the ninjas (Ninjoe, Ninjohn, Ninjerry and the fourth kind: tribes 211, 214, 217, 220,
    with their parts and bombs, 212-222) get the game's own "hidden and frozen" NPC flag (flag46C
    0x20000, as npc_culling uses), so they neither show, move, attack nor collide. Turning the setting
    off brings them back on the spot (the only way out, should some duel turn out to need them beaten).
    They spawn through npcEntryFromTemplate (US 0x801BE720), which about 80 places call, so refusing the
    spawn itself isn't safe.
*/

namespace mod::sammer_ninjas {

// Hides or brings back this frame's ninjas. Call every frame while playing.
void tick();

// Forgets the hidden ones (a new room has new NPCs). Call on every map change.
void onMapChange();

}
