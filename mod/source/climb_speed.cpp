#include <common.h>
#include <wii/os.h>

#include "climb_speed.h"
#include "code_patching.h"
#include "game_addresses.h"
#include "options_menu.h"

namespace mod::climb_speed {

#define BLR 0x4E800020

// Each climbing add, and a stub that does it twice. The adds sit in functions that keep their own
// return address on the stack, so calling a stub from there is free to use the link register.
static struct
{
    u32 * at;
    u32 instruction;  // the game's add, "fadds fD, fD, fB"
    u32 stub[3];      // the add twice, then return
} sAdds[] = {
    {game_addresses::climb_2d_vertical_add_1, 0xEC00082A, {}},  // fadds f0, f0, f1
    {game_addresses::climb_2d_vertical_add_2, 0xEC00082A, {}},
    {game_addresses::climb_2d_sideways_add, 0xEC42F82A, {}},    // fadds f2, f2, f31
    {game_addresses::climb_3d_vertical_add_1, 0xEC00F82A, {}},  // fadds f0, f0, f31
    {game_addresses::climb_3d_vertical_add_2, 0xEC00F82A, {}},
};
static bool sReady;
static bool sFast;

void init()
{
    for (auto & add : sAdds)
    {
        if (*add.at != add.instruction)
        {
            wii::os::OSReport("[spmap] climb: climbing instruction at %p isn't what was expected (%08x), not patched\n",
                              add.at, *add.at);
            return;
        }
    }
    for (auto & add : sAdds)
    {
        add.stub[0] = add.instruction;
        add.stub[1] = add.instruction;
        add.stub[2] = BLR;
        code_patching::refreshCode(add.stub, sizeof(add.stub));
    }
    sReady = true;
}

void tick()
{
    bool fast = options_menu::get(options_menu::OPTION_FAST_CLIMB);
    if (!sReady || fast == sFast)
        return;
    sFast = fast;
    for (auto & add : sAdds)
    {
        if (fast)
            code_patching::writeBranch(add.at, add.stub, true);
        else
            code_patching::writeInstruction(add.at, add.instruction);
    }
}

}
