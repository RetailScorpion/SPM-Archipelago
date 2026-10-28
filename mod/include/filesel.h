#pragma once

/*
    The file select's Pure Hearts.

    Vanilla shows a save's hearts from its single story value: heart 1 from 4 (Merlon's gift), heart
    N + 1 once Chapter N's ending has played. With per-chapter story values that's whatever the map
    the save was made in had, so the mod shows the Pure Hearts the save holds (AP_GSWF_HEART), the
    ones that open its doors.
*/

namespace mod::filesel {

// Replaces the file select's heart test. Call once at boot.
void init();

}
