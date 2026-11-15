#pragma once

#include <common.h>

/*
    Instant elevators (user, 2026-11-05: "the elevators are just crazy slow").

    Fort Francis (3-4): the elevator in ta4_10 and ta4_11 rides up (door E_doa_01, relF 0x13c258) or
    down (door E_doa_02, relF 0x13ca78) in about 46 seconds: doors open, Mario walks in, the car's
    animation plays with him carried along, the doors open again and he walks out. Both rooms' door
    entries now run the mod's own scripts instead: the car's animation jumps to its end (so the car is
    where the ride would have left it, and the next ride either way still works) and Mario is put at
    the other floor's exit, where the ride leaves him.

    Flipside and Flopside's town elevators (relF 0xe54f8 leaving, 0xe5c50 arriving, shared by all 16)
    keep their scripts, but their waits become 0 and the walking in and out is five times faster.

    Flipside and Flopside Tower's elevators (Flipside 0xb5300 up, 0xb6688 down; Flopside 0xd56f8 /
    0xd6508; about 18 seconds each) go between two spots in the same room, so their buttons run the
    mod's scripts instead, which just put Mario at the other end (user's idea, 2026-11-05). During a new
    file's prologue (story value below 11) the vanilla rides play, since they carry its scenes.
*/

namespace mod::elevator_skips {

// Finds the words the fast elevators change (each checked against its vanilla value) and switches them
// on. Call once at startup.
void init();

// Switches the elevators fast or vanilla, following the "Fast elevators" setting (options menu, on by
// default; user, 2026-11-05). Call on every map change, once the save's settings are known.
void onMapChange();

}
