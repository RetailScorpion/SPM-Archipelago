#include <common.h>
#include <spm/nandmgr.h>
#include <wii/os.h>

#include "code_patching.h"
#include "file_select_hearts.h"
#include "game_addresses.h"
#include "save_data.h"

namespace mod::file_select_hearts {

// The game's 4 instructions at filesel_heart_check, which the mod expects to find
static const u32 sExpected[4] = {
    0x80710148,  // lwz r3, 0x148(r17)  -- the save's story value
    0x80150000,  // lwz r0, 0(r21)      -- this heart's threshold
    0x7C030040,  // cmplw r3, r0        -- compare them
    0x41800074,  // blt +0x74           -- lower: skip past the heart's picture
};

// Their replacement: call the mod's test (through the stub below), then skip the picture if it
// returned 0
static const u32 CMPWI_R3_0 = 0x2C030000;   // cmpwi r3, 0     -- did the test return 0?
static const u32 BEQ_SKIP = 0x41820078;     // beq +0x78       -- yes: skip (same target as the blt)
static const u32 NOP = 0x60000000;          // nop             -- do nothing
// The stub: hands the save (r17) and the heart number (r23) to heartShown, then jumps to it
static const u32 MR_R3_R17 = 0x7E238B78;    // mr r3, r17
static const u32 MR_R4_R23 = 0x7EE4BB78;    // mr r4, r23

static u32 sStub[3];

// Whether a save holds Pure Heart number `heart` (0-7, the picture's position).
static s32 heartShown(const spm::nandmgr::SaveFile * save, s32 heart)
{
    s32 flag = AP_GSWF_HEART(heart + 1);
    return (save->spmarioGlobals.gswf[flag / 32] >> (flag % 32)) & 1;
}

// Whether the game's instructions are the ones the mod expects.
static bool instructionsAsExpected(const u32 * at)
{
    for (s32 i = 0; i < 4; i++)
    {
        if (at[i] != sExpected[i])
        {
            wii::os::OSReport("[spmap] filesel: unexpected instruction %08x, not patched\n", at[i]);
            return false;
        }
    }
    return true;
}

void init()
{
    u32 * at = game_addresses::filesel_heart_check;
    if (!instructionsAsExpected(at))
        return;

    sStub[0] = MR_R3_R17;
    sStub[1] = MR_R4_R23;
    code_patching::writeBranch(&sStub[2], reinterpret_cast<void *>(heartShown));
    code_patching::refreshCode(sStub, sizeof(sStub));

    code_patching::writeBranch(&at[0], sStub, true);
    at[1] = CMPWI_R3_0;
    at[2] = BEQ_SKIP;
    at[3] = NOP;
    code_patching::refreshCode(at, 4 * sizeof(u32));
    wii::os::OSReport("[spmap] filesel: Pure Hearts follow each chapter's progress\n");
}

}
