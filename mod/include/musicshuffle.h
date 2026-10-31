#pragma once

#include <common.h>

/*
    Music shuffle and Shuffle fanfares (Options settings): every track plays in place of another, the
    same way each time for a save file (seeded by its name). Looping songs trade with looping songs
    (Music shuffle), tracks that play once (fanfares, chapter starts...) with each other (Shuffle
    fanfares).
*/

namespace mod::musicshuffle {

void init();

// Call every frame in game: lets the game carry on once a shuffled jingle has finished
void tick();

}
