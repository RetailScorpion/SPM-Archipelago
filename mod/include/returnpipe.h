#pragma once

#include <common.h>

/*
    Return Pipe availability.

    Map scripts switch the Return Pipe on and off in the pause menu with func_800d4de4(1, onoff)
    (evt_sub.h; it calls into pluswin): 1 disables it, 0 enables it. Vanilla disables it in boss
    rooms and story scenes, which is harmless, and in Merlee's Mansion (mi3_01-06) while Chapter 2
    is before 89, where in the randomizer you can be stuck without Slim. The mod changes the
    mansion's calls to enable it instead.
*/

namespace mod::returnpipe {

void init();

}
