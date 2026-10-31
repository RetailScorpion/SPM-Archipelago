#pragma once

#include <spm/seqdrv.h>

/*
    The skip_cutscenes option: story scenes the randomizer skips unless the player wants to watch.
      - a new file's opening (Count Bleck and Peach, Mario's house, meeting Tippi) and Merlon's scenes
        after it: the file starts in Flipside Tower, with the Return Pipe
      - the Count Bleck scene after the prologue and after each of Chapters 1-7 (Castle Bleck's
        throne room, map ls4_11), and the playable Peach and Luigi scenes after Chapters 1 and 2
    Each skip sends Mario where the skipped scenes would have, the way the scenes themselves do it:
    by changing which map is being loaded (and, for the prologue, the story value).
    The client passes on the option (mailbox playCutscenes); without a client, scenes are skipped.
*/

namespace mod::cutscene_skips {

// May change the map being loaded (wp->p0 = map, wp->p1 = door) and the story value.
// Call at the very start of every map change, before story_progress::onMapChange.
void onMapChange(spm::seqdrv::SeqWork * wp);

}
