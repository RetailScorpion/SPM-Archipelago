#pragma once

#include <common.h>

/*
    Pixl join scenes as locations.

    In vanilla, a Pixl's join scene only plays while the chapter's story value is before the point
    where it normally happens, and if you somehow missed it, the game hands you the Pixl at the end
    of the section anyway (a "safety net"). The randomizer plays chapters out of order, and a join
    can hold any item, so the mod changes each join's scripts:
      - "story value < N" checks that start a join become "this join's location isn't collected yet"
      - where the scene's step is chosen by the story value (Boomer's chest, Thudley's scene, Fleep's
        outhouse), it reads a save byte the mod fills in instead (onMapChange)
      - the join's "give the Pixl" call gives whatever the location holds instead
      - the safety nets are switched off
      - traps that need the Pixl to get out are removed (Thoreau's room: iron bars and a locked
        exit; Merlee's Mansion: the Return Pipe switched off; Thudley's room: bars closing behind
        Mario; Cudge's shrine: the exit pipe switched off)
      - Slim's room no longer marks his chest opened just because Mario already has Slim
    Story values from a late join scene are ignored by story_progress, so a join can't move its
    chapter backwards.
*/

namespace mod::pixl_joins {

// Patches every join scene's scripts. Call once at startup.
void init();

// Fills in the save bytes the patched join scenes read, and fixes a missing Pixl selection.
// Call on every map change, after story_progress::onMapChange.
void onMapChange();

// Whether Mario has this Pixl (by item id).
bool havePixl(s32 pixl);

// If Mario has Pixls but none is selected, selects the first one. The game's "add item" doesn't
// select a new Pixl, so receiving one while having none would leave none selected.
void repairPixlSelection();

}
