#include <common.h>
#include <msl/string.h>
#include <spm/evt_mario.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario.h>
#include <wii/os.h>

#include "code_patching.h"
#include "pose_waits.h"

namespace mod::pose_waits {

// How long a scene may wait for an animation while someone other than Mario is played (60 per second)
#define MAX_WAIT_FRAMES 120

#define MARIO 0  // mario.h character

// The script that's waiting, and for how many frames so far
static spm::evtmgr::EvtEntry * sWaiter;
static s32 sWaited;

// ---- Poses: the character's own closest animation for one only Mario has ----

static const struct
{
    s8 character;
    const char * pose;
    const char * standIn;
} sStandIns[] = {
#include "pose_table.inc"
};

static const char * standInFor(s32 character, const char * pose)
{
    for (auto & entry : sStandIns)
        if (entry.character == character && msl::string::strcmp(entry.pose, pose) == 0)
            return entry.standIn;
    return nullptr;
}

// evt_mario_set_pose(name, time): with someone else played, a pose they lack becomes their own closest
// one (given to the game's function as the script's arguments, then the script's own put back)
static s32 (*evtMarioSetPoseReal)(spm::evtmgr::EvtEntry * evt, bool isFirstCall);
static s32 evtMarioSetPoseHook(spm::evtmgr::EvtEntry * evt, bool isFirstCall)
{
    s32 character = spm::mario::marioGetPtr()->character;
    if (character == MARIO)
        return evtMarioSetPoseReal(evt, isFirstCall);
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    const char * pose = reinterpret_cast<const char *>(spm::evtmgr_cmd::evtGetValue(evt, args[0]));
    const char * standIn = pose != nullptr ? standInFor(character, pose) : nullptr;
    if (standIn == nullptr)
        return evtMarioSetPoseReal(evt, isFirstCall);
    spm::evtmgr::EvtScriptCode swapped[] = {reinterpret_cast<spm::evtmgr::EvtScriptCode>(standIn), args[1]};
    evt->pCurData = swapped;
    s32 result = evtMarioSetPoseReal(evt, isFirstCall);
    evt->pCurData = args;
    return result;
}

// ---- Waiting for an animation ----

static s32 (*evtMarioWaitAnimReal)(spm::evtmgr::EvtEntry * evt, bool isFirstCall);
static s32 evtMarioWaitAnimHook(spm::evtmgr::EvtEntry * evt, bool isFirstCall)
{
    s32 result = evtMarioWaitAnimReal(evt, isFirstCall);
    if (result != 0 || spm::mario::marioGetPtr()->character == MARIO)
    {
        sWaiter = nullptr;
        return result;
    }
    if (isFirstCall || sWaiter != evt)
    {
        sWaiter = evt;
        sWaited = 0;
    }
    if (++sWaited < MAX_WAIT_FRAMES)
        return result;
    wii::os::OSReport("[spmap] poses: gave up waiting for an animation (character %d)\n",
                      spm::mario::marioGetPtr()->character);
    sWaiter = nullptr;
    return 2;  // done: carry on
}

void init()
{
    evtMarioWaitAnimReal = code_patching::hookFunction(spm::evt_mario::evt_mario_wait_anim, evtMarioWaitAnimHook);
    evtMarioSetPoseReal = code_patching::hookFunction(spm::evt_mario::evt_mario_set_pose, evtMarioSetPoseHook);
}

}
