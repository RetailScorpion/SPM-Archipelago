#pragma once

#include <common.h>

/*
    Tattle checks (the tattle_checks option; user, 2026-11-07): Tatl, a Pixl to find, and one location per
    kind of enemy.

    Tatl is the game's unused Tippi Pixl (item 220, "MONOSHIRI", which has its own Pixl model), renamed.
    Her entry in the Pixl ability table (0x803E7150, Pixl id -> what pressing 1 does) is the game's
    "never used" assert; it's replaced with a function that does nothing, so pressing 1 with her can't
    crash.

    Tattling: Tippi's pointer already shows an enemy's tattle (searchGetNpcMsg builds it from the enemy's
    catch card text, with its HP and attack; Tippi's own line for enemies without a card). With Tatl
    as the current Pixl and the option on (mailbox seedFlags SEED_FLAG_TATTLE_CHECKS), tattling an enemy
    also sets its kind's flag (AP_GSWF_TATTLE_BASE + kind, kinds by catch card: location_flags.h
    TATTLE_CARDS), which the client sends as that location. Its item comes from the server (the client
    never counts a tattle location's item as already given).

    Tatl's pointer (user's spec: like Fleep's): pressing 1 with her runs Fleep's own motion (0x26: the
    Pixl flies about as a cursor moved with the arrow buttons; 1 again uses it, B cancels), with the
    Pixl id it loads (two "li r3, 0xe2") patched to Tatl's when she's out, and the animations her model
    lacks (A_1, A_2) played as her primary one, S_1. When 1 is pressed again (stage 30, the flip),
    tick() sends the motion to its put-away stage (40) instead, finds the NPC nearest her, and shows
    its tattle in a text box with every NPC frozen. Tippi's own pointer also still works with Tatl out.
*/

namespace mod::tattle_checks {

// Renames item 220 to Tatl and makes her ability safe. Call once at startup.
void init();

// Tatl's name and description, or nullptr for other text (game_messages).
const char * message(const char * msgName);

// After each frame: Tatl's tattle when 1 is pressed again in her pointer.
void tick();

}
