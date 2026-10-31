#pragma once

#include <common.h>

/*
    The pause menu's Options button: the game's unused Maps button (button 7), shown below Recipes with
    its own picture (assets/pause/options_button.png, put in pause.tpl by the patcher). It opens a
    window of settings saved with the file (AP_GSW_OPTIONS), changed in game rather than in the YAML.
*/

namespace mod::options {

enum Option
{
    // New options go at the end: each one's saved bit is its position
    OPTION_MUSIC_SHUFFLE,     // every looping song plays in place of another, seeded by the file name
    OPTION_SKIP_TEXT,         // holding 1 mashes through text
    OPTION_FANFARE_SHUFFLE,   // the same for tracks that play once (fanfares, chapter starts...)
    OPTION_COUNT
};

bool get(Option option);

// The mod's text for a message, or nullptr to use the game's
const char * message(const char * msgName);

void init();

}
