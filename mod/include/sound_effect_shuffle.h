#pragma once

#include <common.h>

/*
    The "Shuffle sound effects" and "Low health beeping" settings.

    Every sound effect the game plays goes through one function. The mod hooks it:
      - Shuffle sound effects: some effects play in place of each other. For now that's only the
        player's sounds (characters and Pixls) and the interface's (menus, text boxes, saving),
        each kind among itself. The shuffle is made from the save file's name, like the music's.
      - Effects that loop (a hum, a whoosh that keeps going until something stops it) only swap with
        other looping effects: a looping effect in a one-shot's place would never be stopped. Which
        effects loop was worked out from the game's sound archive by tools/sfxloops.py and is kept
        in sound_effect_loops.h.
      - Low health beeping off: the beeping is silenced, whether or not effects are shuffled.
*/

namespace mod::sound_effect_shuffle {

// Hooks the game's sound effect functions. Call once at startup.
void init();

}
