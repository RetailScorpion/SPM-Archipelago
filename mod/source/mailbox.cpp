#include <common.h>
#include <msl/string.h>

#include "mailbox.h"

namespace mod::mailbox {

Mailbox gMailbox;

void init()
{
    msl::string::memset(&gMailbox, 0, sizeof(gMailbox));
    gMailbox.version = MAILBOX_VERSION;
    gMailbox.modState = STATE_BOOT;
    // magic last, so a client that finds it sees a fully initialised mailbox
    msl::string::memcpy(gMailbox.magic, MAILBOX_MAGIC, sizeof(gMailbox.magic));
}

bool peekReceived(s32 * itemId)
{
    if (gMailbox.recvHandled >= gMailbox.recvCount)
        return false;

    *itemId = gMailbox.recvItems[gMailbox.recvHandled % MAILBOX_RECV_SIZE];
    return true;
}

void commitReceived()
{
    gMailbox.recvHandled++;
}

bool sendLocation(u32 locationId)
{
    if (gMailbox.sentCount - gMailbox.sentAcked >= MAILBOX_SENT_SIZE)
        return false;

    gMailbox.sentLocations[gMailbox.sentCount % MAILBOX_SENT_SIZE] = locationId;
    gMailbox.sentCount++;
    return true;
}

}
