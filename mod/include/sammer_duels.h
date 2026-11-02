#pragma once

#include <spm/seqdrv.h>

/*
    Fewer Sammer Guy duels (the sammer_duels option): the seed picks which gates play.

    Sammer's Kingdom has one duel room per gate, entered from the room before by its left door
    (doa1_l), or at the start of a group by a plain map change.
      - The story has its own room table: gates 1-20 are duels (wa1_02-wa1_21; gate 17 is Forbidden
        Slap, gate 20 Rolling Thwomp). In gates 21-25 (wa1_22-wa1_26) and 26-29 the Sammer Guys give up
        as Mario walks in, King Sammer's Pure Heart scene is between them (wa2_01), and gate 30's stage
        ends the world. Each room has a story value: winning sets Chapter 6 to it, and from it on the
        room counts as beaten.
      - The post-game has 100 duels in four groups of 25: wa1_02-wa1_26 (gates 1-25), then
        wa2_01-wa2_25, wa3_01-wa3_25 and wa4_01-wa4_25. It keeps one counter of the highest gate beaten
        (LSW 1; local save values are cleared whenever the map prefix changes, so per group), and a
        room's duel is only there while its gate is above it.

    The client writes one bitmask per group (story, post-game 1-4) of the gates that play: the
    group's last gate always, the first one too in the story and the post-game's first group (the
    intro duel), and random ones up to N (sammer_gates in the APworld). Entering a room forwards
    takes Mario on to the next gate that plays instead. In the story, Chapter 6 then moves to the
    value of the gate before it, so the skipped rooms count as beaten, and after Rolling Thwomp Mario
    goes straight to gate 25's room (beaten), on to the vanilla save prompt and the Pure Heart scene.
    In the post-game, the counter already makes skipped rooms count as beaten. A group whose mask is
    0 (no client) or has every gate plays as in vanilla.
*/

namespace mod::sammer_duels {

// Sends Mario past the gates that don't play. Call on every map change, before story_progress's (it
// may change which map is loaded).
void onMapChange(spm::seqdrv::SeqWork * wp);

}
