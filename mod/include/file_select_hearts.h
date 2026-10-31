#pragma once

/*
    The Pure Hearts shown on the file select screen.

    Vanilla works out which hearts a save shows from its single story value: heart 1 from 4
    (Merlon's gift), heart N + 1 once Chapter N's ending has played. With each chapter keeping its
    own story value, that would show whatever the map the save was made in had. So the mod shows
    the Pure Hearts the save actually holds (AP_GSWF_HEART), the ones that open its doors.

    How: the file select's "show heart N?" test (4 instructions in the game's code) is replaced
    with a call to the mod's own test.
*/

namespace mod::file_select_hearts {

// Replaces the file select's heart test. Call once at startup.
void init();

}
