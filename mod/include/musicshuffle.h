#pragma once

#include <common.h>

/*
    Music shuffle (Options setting): every track plays in place of another, the same way each time
    for a save file (seeded by its name). Tracks that play once (fanfares, chapter starts...) only
    trade places with each other.
*/

namespace mod::musicshuffle {

void init();

}
