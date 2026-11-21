#pragma once

#include <common.h>

/*
    Catches O'Chunks when he falls out of the world (user, 2026-11-16).

    Beating O'Chunks in 1-3 (he3_04) with Barry knocked him through the floor: he fell forever (height
    about -91,000,000 when read from Dolphin). His defeat scene (relF 0xf6520-0xf6604) makes him hop
    (evt_npc_arc_to) until he's back near the middle, and each hop only ends when he lands, so the scene
    waited forever and the game was stuck. Barry can be early in the randomizer; vanilla never has him
    this soon.

    Every frame, an O'Chunks NPC ("dodon") that falls more than FALL_LIMIT below the last place he
    stood is put back there, so he lands and the scene carries on. Only a fall counts: a script moving
    him in one jump (the cutscene O'Chunks is hidden at height -1000 during the fight) is left alone. Runs for Archipelago and vanilla
    files alike.
*/

namespace mod::boss_fall_guard {

// Puts a fallen O'Chunks back. Call every frame while playing, after the game's frame.
void tick();

// Forgets where he stood. Call on every map change.
void onMapChange();

}
