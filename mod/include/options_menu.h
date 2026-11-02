#pragma once

#include <common.h>

/*
    The pause menu's Options button and window.

    The game has an unused Maps button in its pause menu (button 7). It only appears once a map has
    been found, which never happens in the US game. The mod makes it always appear, below Recipes,
    with RetailScorpion's Options picture (assets/pause/options_button.png, put in place by the
    patcher). Selecting it opens the mod's own window, where each setting can be switched on or off.

    The settings are saved with the file (one bit each in save byte AP_GSW_OPTIONS), so each save
    file keeps its own. They're changed in game, not in the YAML.
*/

namespace mod::options_menu {

// The settings. Each one's position here is its bit in the save, so new ones go at the end.
enum Option
{
    OPTION_MUSIC_SHUFFLE,     // looping songs play in place of each other (music_shuffle)
    OPTION_SKIP_TEXT,         // holding 1 mashes through text (text_skip)
    OPTION_FANFARE_SHUFFLE,   // tracks that play once trade places too (music_shuffle)
    OPTION_SFX_SHUFFLE,       // some sound effects play in place of each other (sound_effect_shuffle)
    OPTION_LOW_HEALTH_SOUND,  // off: no beeping while health is low (sound_effect_shuffle)
    OPTION_FAST_CLIMB,        // ladders and vines climbed twice as fast (climb_speed)
    OPTION_COUNT
};

// Whether a setting is on for the loaded save.
bool get(Option option);

// The mod's text for a game message name (the button's help line), or nullptr to use the game's.
const char * message(const char * msgName);

// Makes the Options button appear and open the mod's window. Call once at startup.
void init();

}
