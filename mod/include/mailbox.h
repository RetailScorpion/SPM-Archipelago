#pragma once

#include <common.h>

/*
    Shared memory block between the mod and the Archipelago client.
    The client finds it by scanning MEM1 for MAILBOX_MAGIC, then reads and writes it through
    Dolphin's memory. The layout is mirrored in client/mailbox.py; keep them in sync.

    Ring buffers: the writer puts an entry at [count % size] and then increments count.
    The reader keeps its own count of entries it has handled.
*/

namespace mod::mailbox {

#define MAILBOX_MAGIC "SPMAP_MAILBOX_v1"
#define MAILBOX_VERSION 12

// debugFlags bits
#define DEBUG_JOINS_GIVE_NOTHING 0x1      // Pixl joins mark their location but don't give the Pixl
#define DEBUG_JOINS_GIVE_PLACEHOLDER 0x2  // Pixl joins give placeholderItem instead of the Pixl
#define MAILBOX_RECV_SIZE 16
// recvItems bit: the item was already given in the game (an item for this world found here); the mod only counts it
#define RECV_ALREADY_GIVEN 0x8000
#define MAILBOX_SENT_SIZE 32

enum ModState : u32
{
    STATE_BOOT = 0,     // mod loaded, not in a save yet
    STATE_TITLE = 1,    // no save loaded: title, game over or file select screen
    STATE_IN_GAME = 2,  // in a map; items can be given
};

struct Mailbox
{
/* 0x00 */ char magic[16];        // written at runtime, so the file copy of the rel doesn't match a scan
/* 0x10 */ u32 version;
/* 0x14 */ u32 modState;          // ModState, written by the mod

    // client -> game: items to give
/* 0x18 */ u32 recvCount;         // written by the client
/* 0x1C */ u32 recvHandled;       // written by the mod
/* 0x20 */ u16 recvItems[MAILBOX_RECV_SIZE]; // SPM item ids

    // game -> client: checked locations
/* 0x40 */ u32 sentCount;         // written by the mod
/* 0x44 */ u32 sentAcked;         // written by the client
/* 0x48 */ u32 sentLocations[MAILBOX_SENT_SIZE];

    // debugging
/* 0xC8 */ void * storyState;     // storyvalue::StoryState, for the client to display
/* 0xCC */ u32 warpRequest;       // client sets 1 after filling warpMap/warpDoor; the mod clears it
/* 0xD0 */ char warpMap[16];      // map to change to
/* 0xE0 */ char warpDoor[32];     // door to enter by; empty = the map's default spawn
/* 0x100 */ u32 debugFlags;       // DEBUG_*, written by the client
/* 0x104 */ u32 placeholderItem;  // item for DEBUG_JOINS_GIVE_PLACEHOLDER; 0 = Gold Bar
/* 0x108 */ u16 joinItems[12];    // per Pixl (index = item id - 220): what its join gives, overriding
                                  // debugFlags; 0 = no override, JOIN_ITEM_OTHER_WORLD = nothing here

    // Archipelago (all written by the mod; the client writes into what they point at)
/* 0x120 */ u32 relfBase;         // address of relF.rel's start (location placements are relF file offsets)
/* 0x124 */ u32 savedRecvCount;   // items from the server the loaded save has handled; 0xFFFFFFFF = no save
/* 0x128 */ void * locNames;      // apitem::LocName[locNamesMax]: names for locations holding other worlds' items
/* 0x12C */ u32 locNamesMax;

    // Options (written by the client from the slot's options)
/* 0x130 */ u32 playCutscenes;    // 1 = play the story scenes the mod can skip (skip_cutscenes off); 0 = skip

    // Story gift locations (hubstory.cpp sGifts order), written by the client: what each
    // gives; 0 = its vanilla item, JOIN_ITEM_OTHER_WORLD = another world's item
/* 0x134 */ u16 giftItems[8];

    // Pure Hearts the seed starts with (bit n - 1 = heart n), written by the client;
    // STARTING_HEARTS_NO_CLIENT until it does, which opens every door without giving any heart
/* 0x144 */ u32 startingHearts;

    // Counts up every frame in a loaded save (written by the mod). The client reads checks only while
    // it moves: a Dolphin reset keeps memory, so this mailbox still says "in game" while the game reboots
/* 0x148 */ u32 gameFrames;
};
static_assert(sizeof(Mailbox) == 0x14C);

#define STARTING_HEARTS_NO_CLIENT 0xFFFFFFFF

// joinItems value: the location holds another world's item
#define JOIN_ITEM_OTHER_WORLD 0xFFFF

extern Mailbox gMailbox;

void init();

/*
    Gets the next item the client has queued, without removing it. Returns false if there's none.
*/
bool peekReceived(s32 * itemId);

/*
    Marks the item from peekReceived as delivered.
*/
void commitReceived();

/*
    Reports a checked location to the client. Returns false if the ring is full (client hasn't acked).
*/
bool sendLocation(u32 locationId);

}
