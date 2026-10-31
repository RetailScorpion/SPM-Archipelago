#include <common.h>
#include <spm/wpadmgr.h>
#include <wii/os.h>
#include <wii/wpad.h>

#include "code_patching.h"
#include "options_menu.h"
#include "text_skip.h"

namespace mod::text_skip {

// PowerPC "bl" (call): the top 6 bits are 18 and the lowest bit is 1; the bits between are the
// distance to the function called
#define CALL_INSTRUCTION_MASK 0xFC000003
#define CALL_INSTRUCTION 0x48000001

/*
    Every place where the text box code asks which buttons were just pressed (US 1.0 addresses).
    They're inside three of the game's functions (named after the European version's list of names):
      - msgMain (0x8002F400): speeds up the text being written out when 1 or 2 is pressed
      - msgWindow_Main (0x80033118) and msgWindow_Clear_Main (0x80033924): move on from (or close) a
        finished text box when 1, 2 or A is pressed
    Yes/no questions are handled by another function (selectWindow_Main, 0x8003440C), which isn't
    changed, so a question still waits for a real button press.
*/
static u32 * const sButtonChecks[] = {
    reinterpret_cast<u32 *>(0x8002F578), reinterpret_cast<u32 *>(0x8002F590),
    reinterpret_cast<u32 *>(0x8002F774), reinterpret_cast<u32 *>(0x8002F78C),
    reinterpret_cast<u32 *>(0x8003341C), reinterpret_cast<u32 *>(0x80033434),
    reinterpret_cast<u32 *>(0x8003347C), reinterpret_cast<u32 *>(0x80033494),
    reinterpret_cast<u32 *>(0x80033710), reinterpret_cast<u32 *>(0x80033728),
    reinterpret_cast<u32 *>(0x800337AC), reinterpret_cast<u32 *>(0x800337C4),
    reinterpret_cast<u32 *>(0x8003382C), reinterpret_cast<u32 *>(0x80033844),
    reinterpret_cast<u32 *>(0x80033A70), reinterpret_cast<u32 *>(0x80033A88),
};

// The buttons the text box code sees as just pressed: the real ones, plus 2 every frame while 1 is
// held (if the setting is on).
static u32 textButtonsPressed(s32 controller)
{
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(controller);
    bool holdingOne = spm::wpadmgr::wpadGetButtonsHeld(controller) & WPAD_BTN_1;
    if (options_menu::get(options_menu::OPTION_SKIP_TEXT) && holdingOne)
        pressed |= WPAD_BTN_2;
    return pressed;
}

// Whether the instruction at `at` is a call to the game's "buttons just pressed" function.
static bool callsButtonsPressed(u32 * at)
{
    u32 instruction = *at;
    if ((instruction & CALL_INSTRUCTION_MASK) != CALL_INSTRUCTION)
        return false;
    // The distance is a signed 26-bit number: shift it to the top and back to keep its sign
    s32 distance = static_cast<s32>(instruction << 6) >> 6 & ~3;
    return reinterpret_cast<u32>(at) + distance == reinterpret_cast<u32>(spm::wpadmgr::wpadGetButtonsPressed);
}

void init()
{
    // Check them all first, so nothing is changed unless every one is as expected
    for (u32 * at : sButtonChecks)
    {
        if (!callsButtonsPressed(at))
        {
            wii::os::OSReport("[spmap] textskip: %p isn't the expected call, not patched\n", at);
            return;
        }
    }
    for (u32 * at : sButtonChecks)
        code_patching::writeBranch(at, reinterpret_cast<void *>(textButtonsPressed), true);
    wii::os::OSReport("[spmap] textskip: hold 1 to skip text ready\n");
}

}
