#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/evt_mario.h>
#include <spm/evt_msg.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario.h>
#include <spm/seqdrv.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "connect_gate.h"
#include "mailbox.h"
#include "save_data.h"

namespace mod::connect_gate {

#define OPENING_DONE 11  // the story value once the opening is over (Flipside Tower)
#define CONNECT_KEY "ap_connect_first"
#define NAME_SIZE 32     // the game's map and door name buffers

// The waiting script is running
static bool sWaiting;
// The current room was set up before the seed was placed
static bool sStale;
// The room is being loaded again: where Mario stood, to put him back
static bool sRestore;
static char sMap[NAME_SIZE];
static char sDoor[NAME_SIZE];
static wii::mtx::Vec3 sPosition;

// Whether this file has been connected before (or is from before the gate).
static bool connectedBefore()
{
    return spm::swdrv::swGet(AP_GSWF_CONNECTED) || spm::swdrv::swGet(AP_GSWF_START_CHARACTER_SET) ||
           save_data::getGswU16(AP_GSW_RECV_COUNT) != 0;
}

static bool playerHasControl()
{
    return !spm::mario::marioCtrlOffChk() && !spm::mario::marioKeyOffChk();
}

// Loads the current room again through the door Mario last came in by (the map's default spawn if
// none), to put him back where he stood once it's loaded.
static void reloadRoom()
{
    auto * gp = spm::spmario::gp;
    msl::string::strcpy(sMap, gp->mapName);
    msl::string::strcpy(sDoor, gp->doorName);
    sPosition = spm::mario::marioGetPtr()->position;
    sRestore = true;
    sStale = false;
    wii::os::OSReport("[spmap] gate: seed placed, loading %s again (door %s)\n", sMap, sDoor[0] ? sDoor : "none");
    spm::seqdrv::seqSetSeq(spm::seqdrv::SEQ_MAPCHANGE, sMap, sDoor[0] ? sDoor : nullptr);
}

// ap_gate_open(&open): open = 1 once the client has placed the seed.
static EVT_DEFINE_USER_FUNC(ap_gate_open)
{
    (void) isFirstCall;
    spm::evtmgr_cmd::evtSetValue(evt, evt->pCurData[0], mailbox::gMailbox.seedPlaced != 0);
    return 2;
}
static constexpr s32 ap_gate_open_parameter_count = 1;

// ap_gate_done(): the wait is over; the room is loaded again (tick, once the player has control).
static EVT_DEFINE_USER_FUNC(ap_gate_done)
{
    (void) evt;
    (void) isFirstCall;
    sWaiting = false;
    return 2;
}
static constexpr s32 ap_gate_done_parameter_count = 0;

EVT_BEGIN(ap_connect_gate_evt)
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(CONNECT_KEY), 0, 0)
    DO(0)
        USER_FUNC(ap_gate_open, LW(0))
        IF_NOT_EQUAL(LW(0), 0)
            DO_BREAK()
        END_IF()
        WAIT_FRM(1)
    WHILE()
    USER_FUNC(spm::evt_mario::evt_mario_key_on)
    USER_FUNC(ap_gate_done)
    RETURN()
EVT_END()

void tick()
{
    if (sRestore && playerHasControl())
    {
        sRestore = false;
        if (msl::string::strcmp(spm::spmario::gp->mapName, sMap) == 0)
            spm::mario::marioGetPtr()->position = sPosition;
    }
    if (sWaiting)
        return;
    if (mailbox::gMailbox.seedPlaced)
    {
        if (!spm::swdrv::swGet(AP_GSWF_CONNECTED))
        {
            spm::swdrv::swSet(AP_GSWF_CONNECTED);
            wii::os::OSReport("[spmap] gate: first connection for this file\n");
        }
        if (sStale && spm::spmario::gp->gsw0 >= OPENING_DONE && playerHasControl())  // never in the opening
            reloadRoom();
        return;
    }
    if (connectedBefore() || spm::spmario::gp->gsw0 < OPENING_DONE || !playerHasControl())
        return;  // the opening, or a scene first: the box comes once the player would have control
    sWaiting = true;
    spm::evtmgr::evtEntry(ap_connect_gate_evt, 0, 0);
    wii::os::OSReport("[spmap] gate: waiting for the server\n");
}

void onMapChange(const char * map)
{
    // only a file's first connection: later loads play on before connecting, as they always have
    sStale = !mailbox::gMailbox.seedPlaced && !connectedBefore();
    if (sRestore && msl::string::strcmp(map, sMap) != 0)
        sRestore = false;
}

void onNoSaveLoaded()
{
    sWaiting = false;
    sStale = false;
    sRestore = false;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, CONNECT_KEY) == 0)
        return "<system>\n\nPlease connect to an\nArchipelago server.\n<k>";
    return nullptr;
}

}
