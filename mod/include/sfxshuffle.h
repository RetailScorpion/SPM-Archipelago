#pragma once

#include <common.h>

/*
    Shuffle sound effects (Options setting): the player's sounds (characters' and Pixls') play in place
    of each other, and so do the interface's (menus, text boxes, saving), the same way each time for a
    save file (seeded by its name); other effects aren't touched for now. Effects that loop swap only with each other (the game
    stops those itself; a looping effect in a one-shot's place would never stop), using the loop table
    tools/sfxloops.py generates from the sound archive (sfxloops_data.h). A TV speaker variant (X_TV)
    follows its base effect.
*/

namespace mod::sfxshuffle {

void init();

}
