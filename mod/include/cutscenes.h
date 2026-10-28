#pragma once

#include <spm/seqdrv.h>

/*
    The skip_cutscenes option: story scenes the randomizer skips unless the player wants to watch.
      - a new file's opening (Count Bleck and Peach, Mario's house, meeting Tippi) and Merlon's
        scenes after it: starts in Flipside Tower with Pure Heart 1 placed and the Return Pipe
      - the Count Bleck scene after the prologue and after each of Chapters 1-7 (Castle Bleck's throne
        room, ls4_11), and the playable Peach and Luigi intermissions after Chapters 1 and 2
    Each skip jumps to where the skipped scenes would have sent Mario, as the scenes themselves do
    (a map change, plus the story value for the prologue). The client sends the option
    (mailbox playCutscenes); without a client the scenes are skipped.
*/

namespace mod::cutscenes {

/*
    Call at the start of every map change, before storyvalue::onMapChange: may change the map
    being loaded (wp->p0, wp->p1) and the story value.
*/
void onMapChange(spm::seqdrv::SeqWork * wp);

}
