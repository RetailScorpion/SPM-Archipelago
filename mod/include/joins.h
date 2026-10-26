#pragma once

#include <common.h>

/*
    Pixl join scenes.

    Vanilla only starts a join scene while the chapter's story value is before the point where it
    normally happens, and gives the Pixl at the end of the section if it was missed (a "safety net",
    party_hoken_NN). With chapters played out of order the story value can pass a join without it
    happening, and the join location can hold any item, so the mod patches each join's scripts:
      - gates "story value < N" become "join location not collected" (in place: both are
        two-argument IF instructions)
      - where a SWITCH on the story value picks the scene's stage (Boomer's chest, Thudley's scene,
        Fleep's outhouse), the SWITCH reads a reserved save byte instead, which the mod fills in on
        every map change: the chapter's value (capped at the join stage) until the join is
        collected, then a value past it
      - the join's add-item call goes through ap_join_gift, which marks the location collected (and,
        until Archipelago is connected, still gives the Pixl)
      - safety nets are disabled
      - traps that need the Pixl to get out are removed (Thoreau's room: iron bars and a locked
        exit; Cudge's shrine: the exit pipe is off at story values 189-192)
      - Slim: the room marks his chest opened if you already have Slim; that's disabled
    storyvalue keeps chapter values from going backwards, since a late join scene still sets the
    story value to its original point.
*/

namespace mod::joins {

void init();

/*
    Fills in the stage values read by the patched SWITCHes. Call on every map change, after
    storyvalue::onMapChange.
*/
void onMapChange();

/*
    If Mario has Pixls but none is selected, selects the first one. pouchAddItem doesn't select a
    new Pixl, so a Pixl received while having none would otherwise leave none selected.
*/
void repairPixlSelection();

}
