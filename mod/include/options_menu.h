#pragma once

#include <common.h>

/*
    The pause menu's Options button and window.

    The game has an unused Maps button in its pause menu (button 7). It only appears once a map has
    been found, which never happens in the US game. The mod makes it always appear, below Recipes,
    with RetailScorpion's Options picture (assets/pause/options_button.png, put in place by the
    patcher). Selecting it opens the mod's own window, where each setting can be switched on or off.

    The window shows 10 settings at a time and scrolls with the cursor; red arrows (the game's
    icon 319) show when there's more above or below.

    The settings are saved with the file (one bit each in save bytes AP_GSW_OPTIONS and
    AP_GSW_OPTIONS_2), so each save
    file keeps its own. They're changed in game, not in the YAML. A new file starts with the settings of
    the last file played (its opening is skipped or not before the menu can be opened).
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
    OPTION_FAST_ELEVATORS,    // elevators skip their rides (elevator_skips)
    OPTION_SKIP_CUTSCENES,    // the story scenes the mod can skip are skipped (cutscene_skips; was a YAML option)
    OPTION_CULL_ENEMIES,      // enemies far off screen are paused and hidden (npc_culling)
    OPTION_SQUIRPS_SHOTS,     // Squirps' guns aren't held to 4 shots out at once (squirps_shots)
    OPTION_NO_SAMMER_NINJAS,  // ninjas hidden and frozen in Sammer's Kingdom (sammer_ninjas)
    OPTION_DORGUY_ANSWERS,    // the Dorguys' right quiz answers in red (dorguy_answers)
    OPTION_SQUIRPS_RAPID,     // holding 2 keeps Squirps firing (squirps_shots)
    OPTION_COUNT
};

// Whether a setting is on for the loaded save.
bool get(Option option);

// Remembers the loaded save's settings, for the next new file. Call on map changes with a save loaded.
void remember();

// Gives a new file the settings remembered last. Call when a new file starts.
void carryOverToNewFile();

// The file select screen gets an Options button between Copy Data and Back (the bottom row squeezed
// closer; user, 2026-11-05). It opens the same window, for the settings the next new file starts
// with, so they can be set before starting one. While it's open, the file select doesn't see the Wii
// Remote's buttons. Call every frame on the file select, before the game's own frame.
void fileSelectTick();

// The mod's text for a game message name (the button's help line), or nullptr to use the game's.
const char * message(const char * msgName);

// Makes the Options button appear and open the mod's window. Call once at startup.
void init();

}
