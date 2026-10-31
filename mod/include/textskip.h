#pragma once

#include <common.h>

/*
    Holding 1 mashes through text when the Options setting is on: the message code sees 2 pressed
    every frame while 1 is held. Choice prompts still wait for a real press.
*/

namespace mod::textskip {

void init();

}
