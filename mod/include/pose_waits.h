#pragma once

#include <common.h>

/*
    Scenes waiting on Mario's animations (user, 2026-11-09: Saffron sometimes froze starting as someone
    else, and Squirps' scenes as Luigi). Story scenes pose Mario directly, e.g. Saffron's first visit:
    evt_mario_set_pose("T_3B") then evt_mario_wait_anim, which waits until the animation ends. Peach,
    Bowser and Luigi lack many of Mario's animations, so that wait never ended. While someone other than
    Mario is played, evt_mario_wait_anim (US 0x800F1818) now gives up after MAX_WAIT_FRAMES and lets the
    scene carry on; with Mario, or once the animation ends, it's vanilla.

    And the poses themselves (user, 2026-11-09): evt_mario_set_pose (US 0x800F17AC) gives a character who
    lacks the pose their own closest animation instead (pose_table.inc, made from the animation files:
    the same pose without its variant letter, T_3B -> T_3A, then the same family, T_13 -> T_1, else S_1).
    40 swaps over 60 poses scenes use; the two-second limit stays as a backstop.
*/

namespace mod::pose_waits {

// Hooks evt_mario_wait_anim and evt_mario_set_pose. Call once at startup.
void init();

}
