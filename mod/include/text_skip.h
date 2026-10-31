#pragma once

#include <common.h>

/*
    The "Hold 1 to skip text" setting.

    In vanilla, pressing 1 or 2 speeds up text and moves on to the next text box; you have to press
    it again for every box. With this setting on, holding 1 does it for you: the text box code sees
    2 pressed every frame while 1 is held. Yes/no questions still wait for a real button press.
*/

namespace mod::text_skip {

// Points the text box code's button checks at the mod's version. Call once at startup.
void init();

}
