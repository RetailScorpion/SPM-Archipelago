#pragma once

#include <common.h>

/*
    The "Music shuffle" and "Shuffle fanfares" settings.

    Every song the game starts goes through one function. The mod hooks it and plays another track
    instead. Which track replaces which is a shuffle made from the save file's name, so a file always
    gets the same shuffle, and files with different names get different ones.
      - Music shuffle: looping songs (maps, battles, story scenes) trade places with each other
      - Shuffle fanfares: tracks that play once (item get, level up, chapter starts...) trade places
        with each other, and a replacement plays to the end before the game moves on
*/

namespace mod::music_shuffle {

// Hooks the game's music functions. Call once at startup.
void init();

// Lets the game carry on once a held jingle has finished. Call every frame in game.
void tick();

}
