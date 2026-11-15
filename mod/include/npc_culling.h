#pragma once

#include <common.h>

/*
    Culling far enemies (the "Cull far enemies" setting, on by default; user, 2026-11-05: big rooms
    full of enemies, like 7-3's Overthere Stair, slow down in Dolphin, vanilla too).

    Every enemy runs its AI, animation, collision and drawing every frame wherever it is. Enemies the
    room's setup file placed that are well off screen from Mario get the game's own "hidden and frozen"
    NPC flag (flag46C 0x20000), and lose it again once Mario comes back near them (with a margin, so
    ones at the edge don't flicker). Off screen: more than CULL_ACROSS units across (along x in 2D,
    or z when flipped to 3D, whichever is further) or CULL_UPDOWN up or down.

    Only the mod's own flags are ever cleared: an enemy the game has hidden or frozen itself is left
    alone, and so are NPCs not from the setup file (story characters, scripted spawns) and parts of
    bigger enemies.
*/

namespace mod::npc_culling {

// Culls or brings back enemies for this frame. Call every frame in game, before the game's own frame.
void tick();

// Forgets which enemies the mod culled (a new room has new NPCs). Call on every map change.
void onMapChange();

}
