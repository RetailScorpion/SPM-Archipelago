#pragma once

#include <common.h>

/*
    Randomizer data kept in the save file.

    The whole SpmarioGlobals (GSW, GSWF) is written to the save, so the mod reserves ranges the game
    never uses. A scan of every script in main.dol and relF, plus every native swByteGet/swSet call
    and the coin flag table, found nothing above GSW 534 or GSWF 1327.
*/

namespace mod::savedata {

// GSW bytes 1792-2047 (256 bytes)
#define AP_GSW_BASE 1792
#define AP_GSW_STORY_SLOTS (AP_GSW_BASE + 0)  // 8 chapters x 2 bytes (big-endian), 0 = chapter not started
#define AP_GSW_JOIN_STAGES (AP_GSW_BASE + 16) // story value seen by patched join SWITCHes (joins.cpp)
#define AP_GSW_PEACH_SCENE (AP_GSW_BASE + 19) // story value Flipside Tower's patched Peach CASE compares with (hubstory.cpp)
#define AP_GSW_RECV_COUNT (AP_GSW_BASE + 20)  // 2 bytes (big-endian): items from the server handled by this save
#define AP_GSW_WATCHITT (AP_GSW_BASE + 23)    // value Watchitt's patched "go get help" CASE compares with (joins.cpp)
#define AP_GSW_LESSON (AP_GSW_BASE + 22)      // story value Bestovius's patched talk script compares with (hubstory.cpp)
#define AP_GSW_PEACH_CHAIN (AP_GSW_BASE + 24) // story value (60-63) Peach's patched revival scripts read and set (joins.cpp, hubstory.cpp)
// Its values, as vanilla's story value: 60 unconscious in mac_01, 61 Saffron asked, 62 Spicy Soup
// cooked, 63 joined
#define PEACH_CHAIN_START 60
#define PEACH_CHAIN_SOUP 62
#define PEACH_CHAIN_DONE 63

// GSWF flags 7168-8191 (1024 flags)
#define AP_GSWF_BASE 7168
#define AP_GSWF_FLIP (AP_GSWF_BASE + 0)  // Mario can flip
// Pixl join locations collected (one flag each, in item order: Thoreau 221 .. Dottie 228)
#define AP_GSWF_JOIN_BASE (AP_GSWF_BASE + 1)
#define AP_GSWF_JOIN(pixlItemId) (AP_GSWF_JOIN_BASE + (pixlItemId) - 221)
// Unflagged key item spots, which the mod gives these as their save flag (locations.cpp)
#define AP_GSWF_LOC_BASE (AP_GSWF_BASE + 32)
// Story gift locations (hubstory.cpp sGifts order: Peach, Luigi, Helmet, Bowser, Bestovius), one flag each
#define AP_GSWF_GIFT_BASE (AP_GSWF_BASE + 16)
#define AP_GSWF_GIFT(index) (AP_GSWF_GIFT_BASE + (index))
// Pure Hearts held, one flag each (heart 1-8); heart n opens tower door n (doors.cpp)
#define AP_GSWF_HEART_BASE (AP_GSWF_BASE + 24)
#define AP_GSWF_HEART(n) (AP_GSWF_HEART_BASE + (n) - 1)

// Vanilla flags the mod relies on
#define GSWF_TIPPI_PRESENT 403  // written by the save code from the guide state, restored on load

u8 getGswByte(s32 id);
void setGswByte(s32 id, u8 value);

}
