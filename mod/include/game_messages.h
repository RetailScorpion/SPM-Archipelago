#pragma once

#include <common.h>

/*
    The mod's own text in the game.

    The game looks up every piece of text by a name (for example "item_get" for "You got X!"),
    through one function, msgSearch. The mod hooks that function once, here, and asks each feature
    in turn whether it has its own text for the name; if none does, the game's text is used.
    (A game function can only be hooked once, so all features share this one hook.)
*/

namespace mod::game_messages {

// Hooks the game's text lookup. Call once at startup.
void init();

}
