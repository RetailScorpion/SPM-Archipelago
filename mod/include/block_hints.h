#pragma once

/*
    The block codes in 5-1 (Downtown of Crag): two rooms where three blocks (dokan_sw1-3, left to
    right) must be hit in a set order to raise a pipe. A coin floats over the block to hit next
    (user's request), until the pipe is up.

      - gn1_02: left, right, middle. The room's hit scripts keep the progress in LSW(1) (0, 1, 2; a
        wrong hit sets 0). Solved: Chapter 5 at 183.
      - gn1_03: a 25-hit code. Native code in relF keeps the last 25 hits (block 0-2, oldest first
        from the write position) in a ring buffer in relF's bss (+0x250, write position at +0x2B4),
        and the pipe rises when they match the code (relF section 5 +0xB4BA8, 25 words). The next
        block is the code's entry after the longest run of recent hits matching its start, so a
        wrong hit never loses progress the game would still count. Solved: Chapter 5 at 185.

    The coin is the game's own item icon, placed with the icon system the game uses for items lying
    around (iconEntry, then its position).
*/

namespace mod::block_hints {

// Notes which room is being entered. Call on every map change.
void onMapChange(const char * nextMap);

// Moves the coin over the next block. Call every frame in game.
void tick();

}
