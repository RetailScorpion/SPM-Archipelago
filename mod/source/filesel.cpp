#include <common.h>
#include <spm/nandmgr.h>
#include <wii/os.h>

#include "filesel.h"
#include "gamesyms.h"
#include "patch.h"
#include "savedata.h"

namespace mod::filesel {

// The instructions at filesel_heart_check
static const u32 sExpected[4] = {
    0x80710148,  // lwz r3, 0x148(r17)
    0x80150000,  // lwz r0, 0(r21)
    0x7C030040,  // cmplw r3, r0
    0x41800074,  // blt +0x74 (past the heart's icon)
};

/*
    Replacement test: bl to a stub that passes the save (r17) and heart index (r23) on to
    heartShown, then skip the icon when it returns 0.
*/
static const u32 CMPWI_R3_0 = 0x2C030000;
static const u32 BEQ_SKIP = 0x41820078;  // the blt's target, one instruction further back
static const u32 NOP = 0x60000000;
static const u32 MR_R3_R17 = 0x7E238B78;
static const u32 MR_R4_R23 = 0x7EE4BB78;

static u32 sStub[3];

// heart: 0-7, the icon's index
static s32 heartShown(const spm::nandmgr::SaveFile * save, s32 heart)
{
    s32 flag = AP_GSWF_HEART(heart + 1);
    return (save->spmarioGlobals.gswf[flag / 32] >> (flag % 32)) & 1;
}

void init()
{
    u32 * at = gamesyms::filesel_heart_check;
    for (s32 i = 0; i < 4; i++)
    {
        if (at[i] != sExpected[i])
        {
            wii::os::OSReport("[spmap] filesel: unexpected instruction %08x, not patched\n", at[i]);
            return;
        }
    }

    sStub[0] = MR_R3_R17;
    sStub[1] = MR_R4_R23;
    patch::writeBranch(&sStub[2], reinterpret_cast<void *>(heartShown));
    wii::os::DCFlushRange(sStub, sizeof(sStub));
    wii::os::ICInvalidateRange(sStub, sizeof(sStub));

    patch::writeBranch(&at[0], sStub, true);
    at[1] = CMPWI_R3_0;
    at[2] = BEQ_SKIP;
    at[3] = NOP;
    wii::os::DCFlushRange(at, 4 * sizeof(u32));
    wii::os::ICInvalidateRange(at, 4 * sizeof(u32));
    wii::os::OSReport("[spmap] filesel: Pure Hearts follow each chapter's progress\n");
}

}
