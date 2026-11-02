#pragma once

/*
    Fast climbing (an Options setting, on by default): Mario climbs ladders and vines twice as fast.

    Climbing is Mario's motion 13. Each frame it reads the stick and adds its direction (1 unit at full
    tilt) to his climbing move, which the movement code then applies: up and down at mario+0x138,
    sideways at mario+0x134. With the setting on, each of those adds (in the 2D and 3D climbing code)
    calls a stub that does the same add twice; with it off, the game's instructions are put back. The
    ends of the ladder are still the game's own checks.
*/

namespace mod::climb_speed {

// Checks the climbing instructions. Call once at startup.
void init();

// Follows the setting. Call every frame while playing.
void tick();

}
