#pragma once

#include <spm/seqdrv.h>

/*
    The "Skip cutscenes" setting (options menu, on by default; a YAML option, skip_cutscenes, until
    2026-11-05): story scenes the randomizer skips unless the player wants to watch.
      - a new file's opening (Count Bleck and Peach, Mario's house, meeting Tippi) and Merlon's scenes
        after it: the file starts in Flipside Tower, with the Return Pipe
      - the Count Bleck scene after the prologue and after each of Chapters 1-7 (Castle Bleck's
        throne room, map ls4_11), and the playable Peach and Luigi scenes after Chapters 1 and 2
      - King Sammer's scenes before and after the first Sammer Guy duel (wa1_02): the room becomes
        an ordinary duel room, so the duel stays
      - Dimentio and Mr. L's scene after Brobot L-type (wa1_27): straight to the save prompt
      - Tippi's welcome and pointing tutorial at the start of 1-1 (he1_01): the title card stays
    Each skip sends Mario where the skipped scenes would have, the way the scenes themselves do it:
    by changing which map is being loaded (and, for the prologue, the story value).
    A new file has no settings of its own yet when its opening starts: it takes the last played file's
    (options_menu::carryOverToNewFile), so turning skipping off on any file lets the next new file play
    its opening.
*/

namespace mod::cutscene_skips {

// Whether the skippable scenes play (the setting is off).
bool playCutscenes();

// Patches the first Sammer duel room so its scenes can be skipped. Call once at startup.
void init();

// May change the map being loaded (wp->p0 = map, wp->p1 = door) and the story value.
// Call at the very start of every map change, before story_progress::onMapChange.
void onMapChange(spm::seqdrv::SeqWork * wp);

}
