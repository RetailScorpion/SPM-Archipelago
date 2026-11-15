#pragma once

#include <common.h>

/*
    What Chapter 8's door and the final battle need (the castle_requirement and final_requirement
    options). Each is one of:
      - ENDGAME_VANILLA: Pure Heart 8
      - ENDGAME_CHAPTERS: a number of Chapters 1-7 finished
      - ENDGAME_PURE_HEARTS: a number of Pure Hearts held (1-8; the starting doors' hearts
        count unless they're items to find, mailbox startingHeartsInPool)
      - ENDGAME_OPEN (Chapter 8's door only): nothing
    The client writes them into the mailbox. Without a client the door needs Chapters 1-7 finished,
    as before, and the battle needs nothing.

    Chapter 8's door: Flopside Tower turns it on when the requirement is met (hub_story_fixes).

    The final battle: Count Bleck's throne room (ls4_11) is entered from the staircase room in 8-4
    (ls4_10, door doa2_l). There the mod puts a padlock on that door, opened by the Throne Room Key,
    and a sign next to it saying what's still needed. The key is the second Goldfish Bowl (item 23)
    under a new name and the Castle Bleck Key's picture: its only use, the goldfish trade in
    Flipside, never happens with the hub's story value fixed low. Mario gets it (an item-get) on the
    first map change once the requirement is met. A separate key, so none of the castle's own keys can
    open the door early. Once opened (its own save flag), the padlock and sign stay gone.
*/

namespace mod::endgame_requirements {

enum Kind
{
    ENDGAME_NONE = 0,  // no client
    ENDGAME_VANILLA = 1,
    ENDGAME_CHAPTERS = 2,
    ENDGAME_PURE_HEARTS = 3,
    ENDGAME_OPEN = 4,  // Chapter 8's door only: open from the start
};

// Turns item 23 into the Throne Room Key. Call once at startup.
void init();

// Whether Chapter 8's door is open.
bool castleOpen();

// Gives the Throne Room Key once the final requirement is met, and notes when the staircase room
// is being entered. Call on every map change, after story_progress::onMapChange.
void onMapChange(const char * nextMap);

// Puts the padlock and sign in the staircase room once it's loaded. Call every frame in game.
void tick();

// The mod's text for a game message name, or nullptr to use the game's own.
const char * message(const char * msgName);

}
