#include <common.h>
#include <spm/wpadmgr.h>
#include <wii/os.h>
#include <wii/wpad.h>

#include "options.h"
#include "patch.h"
#include "textskip.h"

namespace mod::textskip {

/*
    The "bl wpadGetButtonsPressed" calls of the message code (US 1.0; EU0 names, US = EU0 - 0x11C):
    msgMain (0x8002F400) speeds up the text being printed on 1 or 2; msgWindow_Main (0x80033118) and
    msgWindow_Clear_Main (0x80033924) advance or close a finished box on 1, 2 or A. Choice prompts
    are selectWindow_Main (0x8003440C), which isn't touched, so a choice still needs a real press.
*/
static u32 * const sCalls[] = {
    reinterpret_cast<u32 *>(0x8002F578), reinterpret_cast<u32 *>(0x8002F590),
    reinterpret_cast<u32 *>(0x8002F774), reinterpret_cast<u32 *>(0x8002F78C),
    reinterpret_cast<u32 *>(0x8003341C), reinterpret_cast<u32 *>(0x80033434),
    reinterpret_cast<u32 *>(0x8003347C), reinterpret_cast<u32 *>(0x80033494),
    reinterpret_cast<u32 *>(0x80033710), reinterpret_cast<u32 *>(0x80033728),
    reinterpret_cast<u32 *>(0x800337AC), reinterpret_cast<u32 *>(0x800337C4),
    reinterpret_cast<u32 *>(0x8003382C), reinterpret_cast<u32 *>(0x80033844),
    reinterpret_cast<u32 *>(0x80033A70), reinterpret_cast<u32 *>(0x80033A88),
};

// What the message code sees as pressed: holding 1 counts as pressing 2 every frame
static u32 messageButtonsPressed(s32 controller)
{
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(controller);
    if (options::get(options::OPTION_SKIP_TEXT) && (spm::wpadmgr::wpadGetButtonsHeld(controller) & WPAD_BTN_1))
        pressed |= WPAD_BTN_2;
    return pressed;
}

static bool callsButtonsPressed(u32 * at)
{
    u32 ins = *at;
    if ((ins & 0xFC000003) != 0x48000001)
        return false;
    s32 offset = static_cast<s32>(ins << 6) >> 6 & ~3;
    return reinterpret_cast<u32>(at) + offset == reinterpret_cast<u32>(spm::wpadmgr::wpadGetButtonsPressed);
}

void init()
{
    for (u32 * at : sCalls)
    {
        if (!callsButtonsPressed(at))
        {
            wii::os::OSReport("[spmap] textskip: %p isn't the expected call, not patched\n", at);
            return;
        }
    }
    for (u32 * at : sCalls)
        patch::writeBranch(at, reinterpret_cast<void *>(messageButtonsPressed), true);
    wii::os::OSReport("[spmap] textskip: hold 1 to skip text ready\n");
}

}
