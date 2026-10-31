#pragma once

#include <common.h>

/*
    The mailbox: a block of memory the mod and the Archipelago client share.

    The client runs on the PC and can read and write the game's memory through Dolphin. It finds the
    mailbox by searching memory for MAILBOX_MAGIC, then:
      - writes items from the server into it, for the mod to give to Mario
      - reads which locations the mod has seen checked
      - writes the seed's settings (cutscenes, starting Pure Hearts, what each special location holds)

    The layout below is copied in apworld/spm/game_link.py. Every field must stay at the same place
    (the numbers in the left margin are byte offsets); changing one means changing both sides and
    MAILBOX_VERSION.

    Item and location lists are "ring buffers": a fixed-size list used in a circle. The writer puts
    entry number N at position N % size and then adds 1 to its count; the reader keeps its own count
    of entries it has dealt with, and anything between the two counts is waiting.
*/

namespace mod::mailbox {

#define MAILBOX_MAGIC "SPMAP_MAILBOX_v1"
#define MAILBOX_VERSION 12

#define MAILBOX_RECV_SIZE 16  // items from the server waiting to be given
#define MAILBOX_SENT_SIZE 32  // locations waiting to be read by the client

// recvItems: this bit marks an item that was already given in the game (one of this world's items
// found here); the mod only counts it as handled
#define RECV_ALREADY_GIVEN 0x8000

// debugFlags bits, for testing without a seed
#define DEBUG_JOINS_GIVE_NOTHING 0x1      // Pixl joins mark their location but give nothing
#define DEBUG_JOINS_GIVE_PLACEHOLDER 0x2  // Pixl joins give placeholderItem instead of the Pixl

// What the mod is doing right now (modState)
enum ModState : u32
{
    STATE_BOOT = 0,     // mod loaded, no save loaded yet
    STATE_TITLE = 1,    // no save loaded: title screen, game over or file select
    STATE_IN_GAME = 2,  // playing in a map: items can be given
};

struct Mailbox
{
/* 0x00 */ char magic[16];        // filled in at startup, so the mod file itself doesn't match a search
/* 0x10 */ u32 version;
/* 0x14 */ u32 modState;          // ModState, written by the mod

    // Client -> game: items from the server to give
/* 0x18 */ u32 recvCount;         // written by the client
/* 0x1C */ u32 recvHandled;       // written by the mod
/* 0x20 */ u16 recvItems[MAILBOX_RECV_SIZE]; // game item ids

    // Game -> client: locations (only the developer tools read these now)
/* 0x40 */ u32 sentCount;         // written by the mod
/* 0x44 */ u32 sentAcked;         // written by the client
/* 0x48 */ u32 sentLocations[MAILBOX_SENT_SIZE];

    // Developer tools
/* 0xC8 */ void * storyState;     // story_progress::StoryState, for the tools to display
/* 0xCC */ u32 warpRequest;       // a tool sets 1 after filling warpMap/warpDoor; the mod clears it
/* 0xD0 */ char warpMap[16];      // map to warp to
/* 0xE0 */ char warpDoor[32];     // door to arrive by; empty = the map's default spot
/* 0x100 */ u32 debugFlags;       // DEBUG_* bits
/* 0x104 */ u32 placeholderItem;  // item for DEBUG_JOINS_GIVE_PLACEHOLDER; 0 = Gold Bar
/* 0x108 */ u16 joinItems[12];    // what each Pixl's join gives (index = item id - 220); 0 = no
                                  // override, JOIN_ITEM_OTHER_WORLD = another world's item

    // Archipelago: the mod fills these in, the client writes into what they point at
/* 0x120 */ u32 relfBase;         // where the game's relF.rel file starts in memory (item placements
                                  // are written at offsets into that file)
/* 0x124 */ u32 savedRecvCount;   // items from the server the loaded save has handled; 0xFFFFFFFF = no save
/* 0x128 */ void * locNames;      // archipelago_item::LocName[locNamesMax]: item and player names for
                                  // locations holding other worlds' items
/* 0x12C */ u32 locNamesMax;

    // Seed settings, written by the client
/* 0x130 */ u32 playCutscenes;    // 1 = play the story scenes the mod can skip; 0 = skip them

    // What each story gift location holds (story_gifts order), written by the client:
    // 0 = its vanilla item, JOIN_ITEM_OTHER_WORLD = another world's item
/* 0x134 */ u16 giftItems[8];

    // The Pure Hearts the seed starts with (bit N - 1 = heart N), written by the client.
    // STARTING_HEARTS_NO_CLIENT until then, which opens every door without giving any heart
/* 0x144 */ u32 startingHearts;

    // Counts up every frame while a save is loaded. The client only reads checks while it moves:
    // Dolphin's "reset" keeps memory, so the old mailbox still says "in game" while the game reboots
/* 0x148 */ u32 gameFrames;
};
static_assert(sizeof(Mailbox) == 0x14C);

#define STARTING_HEARTS_NO_CLIENT 0xFFFFFFFF

// joinItems / giftItems value: the location holds another world's item
#define JOIN_ITEM_OTHER_WORLD 0xFFFF

extern Mailbox gMailbox;

// Clears the mailbox and writes its magic, version and starting values. Call once at startup.
void init();

// Looks at the next item from the server without taking it. Returns false if none is waiting.
bool peekReceived(s32 * itemId);

// Takes the item peekReceived returned off the list (it has been given).
void commitReceived();

// Puts a location on the list for the client. Returns false if the list is full.
bool sendLocation(u32 locationId);

}
