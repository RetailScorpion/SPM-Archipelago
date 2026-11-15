#pragma once

#include <common.h>

/*
    "Remove Squirps projectile limit" (Options setting, user 2026-11-08). In the space levels (Chapter 4),
    Squirps may only have 4 shots out at once: firing again does nothing until one is gone.

    The game has two shot blocks. The normal one (0xd0 bytes: 0x10, then 4 slots of 0x30), which Squirps
    uses in 4-1 (user, 2026-11-09: patching only the space one did nothing): fired through the caller at
    0x80113f40 (4 slots), counted by the same two counts (4 there). The space mode one, below.

    How the game keeps them (main.dol, US 1.0): Mario's shot work (MarioWork + 0x35c) points at a
    0x4fc-byte block in space mode (0x8011362c), whose shots start at +0x10 + 0x6c: 24 slots of 0x30
    bytes, 0x120 per gun (4 shots, then 2 for its special shot), up to 4 guns. The routine that fires a
    shot (0x8011396c) looks for a free slot among the ones it's given; the space caller (0x80113fd0)
    gives it the gun's own 4. Everything else (moving, drawing, hitting, clearing) goes over all the
    slots, through a count of 24 (0x80114c74, 0x80114f84, the clear loop at 0x80113540).

    The mod always makes both blocks EXTRA_SHOTS slots bigger (zeroed, the counts raised to match), which
    changes nothing by itself: the extra slots are always free. With the setting on, the normal block's
    fire routine is offered all its slots, and the space caller the extra ones instead of the gun's 4.
*/

/*
    "Hold 2 for rapid fire Squirps" (Options setting, user 2026-11-12): while 2 is held in 4-1 or 4-3
    (sp1_ and sp3_ maps) in Squirps' fire mode, Mario's 2-held frame count (MarioWork held2Time) is put
    back to 1 every RAPID_FIRE_FRAMES frames before the game's frame; the game fires each time it reaches 2.
*/

namespace mod::squirps_shots {

// Rapid fire: call every frame while playing, before the game's own frame.
void rapidFireTick();


// Makes the room for the extra shots. Call once at startup.
void init();

// Follows the setting: shots go to the extra slots while it's on. Call every frame while playing.
void tick();

}
