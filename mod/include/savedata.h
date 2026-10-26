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
#define AP_GSW_RECV_COUNT (AP_GSW_BASE + 20)  // 2 bytes (big-endian): items from the server handled by this save

// GSWF flags 7168-8191 (1024 flags)
#define AP_GSWF_BASE 7168
#define AP_GSWF_FLIP (AP_GSWF_BASE + 0)  // Mario can flip
// Pixl join locations collected (one flag each, in item order: Thoreau 221 .. Dottie 228)
#define AP_GSWF_JOIN_BASE (AP_GSWF_BASE + 1)
#define AP_GSWF_JOIN(pixlItemId) (AP_GSWF_JOIN_BASE + (pixlItemId) - 221)
// Unflagged key item spots, which the mod gives these as their save flag (locations.cpp)
#define AP_GSWF_LOC_BASE (AP_GSWF_BASE + 32)

// Vanilla flags the mod relies on
#define GSWF_TIPPI_PRESENT 403  // written by the save code from the guide state, restored on load

u8 getGswByte(s32 id);
void setGswByte(s32 id, u8 value);

}
