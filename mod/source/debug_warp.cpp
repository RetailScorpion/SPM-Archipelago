#include <common.h>
#include <spm/mario.h>
#include <spm/seqdrv.h>
#include <wii/os.h>

#include "debug_warp.h"
#include "mailbox.h"
#include "received_items.h"

namespace mod::debug_warp {

// Frames in a row the player must have had control before warping (60 = one second)
#define CONTROL_FRAMES_NEEDED 60

static s32 sControlFrames;

// Counts the frames in a row the player has had control (control briefly comes back in the
// middle of an item-get, so one frame isn't enough).
static void countControlFrames()
{
    if (spm::mario::marioCtrlOffChk() || spm::mario::marioKeyOffChk())
        sControlFrames = 0;
    else
        sControlFrames++;
}

void tick()
{
    mailbox::Mailbox & mb = mailbox::gMailbox;
    if (mb.warpRequest == 0 || spm::seqdrv::seqGetSeq() != spm::seqdrv::SEQ_GAME)
        return;

    countControlFrames();
    if (sControlFrames < CONTROL_FRAMES_NEEDED || received_items::isBusy())
        return;

    mb.warpRequest = 0;
    // Make sure the names end inside their boxes, whatever the tool wrote
    mb.warpMap[sizeof(mb.warpMap) - 1] = '\0';
    mb.warpDoor[sizeof(mb.warpDoor) - 1] = '\0';
    wii::os::OSReport("[spmap] debug warp to %s (%s)\n", mb.warpMap, mb.warpDoor);
    spm::seqdrv::seqSetSeq(spm::seqdrv::SEQ_MAPCHANGE, mb.warpMap, mb.warpDoor[0] ? mb.warpDoor : nullptr);
}

}
