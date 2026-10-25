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
#define MAILBOX_VERSION 1
#define MAILBOX_RECV_SIZE 16
#define MAILBOX_SENT_SIZE 32

enum ModState : u32
{
    STATE_BOOT = 0,     // mod loaded, not in a save yet
    STATE_TITLE = 1,    // on the title or file select screen
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
};
static_assert(sizeof(Mailbox) == 0xC8);

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
