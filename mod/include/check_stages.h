#pragma once

/*
    Checks that stay obtainable whatever the story value (user, 2026-11-11).

    Sixteen locations only exist while their chapter's story value is low enough: their spawn, box or
    scene sits under a story test (found by tools/story_gates.py and tools/talk_swaps.py, which follows each location's whole
    call chain from its map's setup). With items arriving early, the story can pass them: a lock opened
    before its own key's spot, or rooms walked through past a scene (most rooms move the story on when
    first entered). Those checks would be gone for good.

    Each such check gets its own "stage", an evt global word (GW 16-31, which no game script uses; all of them now) that
    its tests read instead of the story value. The mod sets it on every map change:
      - collected: the chapter's value, but past the check (so the rooms behave as after it)
      - not collected, chapter still inside the check's window: the chapter's value (vanilla)
      - not collected, chapter past it: the check's first step, so its scene plays from the start
    The scenes' own story sets become raise-only (never lowering a story already past them). Checks
    whose scene has several steps over more than one visit (7-2's sisters, 7-4's angel) keep the step
    reached in a save byte, which those sets update.
*/

namespace mod::check_stages {

// Points the checks' story tests at their stages. Call once at startup.
void init();

// Sets every check's stage from the save. Call on every map change, after story_progress::onMapChange.
void onMapChange();

}
