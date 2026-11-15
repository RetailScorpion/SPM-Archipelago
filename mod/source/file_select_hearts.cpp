#include <common.h>
#include <spm/icondrv.h>
#include <spm/nandmgr.h>
#include <wii/mtx.h>
#include <wii/os.h>

#include "code_patching.h"
#include "file_select_hearts.h"
#include "game_addresses.h"
#include "game_mode.h"
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
// The stub: hands the save (r17), the heart number (r23), the game's threshold for it (r21) and the
// file's box position (f30, f29) to heartShown, then jumps to it. (The game sets f1 and f2 afresh
// after the test, so they're free.)
static const u32 MR_R3_R17 = 0x7E238B78;    // mr r3, r17
static const u32 MR_R4_R23 = 0x7EE4BB78;    // mr r4, r23
static const u32 MR_R5_R21 = 0x7EA5AB78;    // mr r5, r21
static const u32 FMR_F1_F30 = 0xFC20F090;   // fmr f1, f30
static const u32 FMR_F2_F29 = 0xFC40E890;   // fmr f2, f29

static u32 sStub[6];

/*
    The Archipelago icon by an Archipelago file's name (user, 2026-11-08). The game draws a file's box
    around (x, y): its name plate centred at (x - 28, y + 55), the name from x - 108 (at most 160
    wide), and the hearts below at the scale used here, with the same icon call (flags 0x4010).
    The icon goes in the box's top-right corner, just under the end of the name plate.
*/
#define AP_ICON 402              // the colour Archipelago icon, added by the patcher
#define ICON_FLAGS 0x4010        // as the hearts
#define ICON_SCALE 0.65f         // a little smaller than the hearts (0.75), to fit the corner
// In the box's top-right corner, under the name plate's end (user, 2026-11-08: moved from beside the
// plate, about 22 right and 20 down)
#define ICON_OFFSET_X 112.0f
#define ICON_OFFSET_Y 35.0f

static void drawArchipelagoIcon(f32 x, f32 y)
{
    wii::mtx::Mtx34 at, scale;
    wii::mtx::PSMTXTrans(at, x + ICON_OFFSET_X, y + ICON_OFFSET_Y, 0.0f);
    wii::mtx::PSMTXScale(scale, ICON_SCALE, ICON_SCALE, ICON_SCALE);
    wii::mtx::PSMTXConcat(at, scale, at);
    spm::icondrv::iconDispGxCol(at, ICON_FLAGS, AP_ICON, {255, 255, 255, 255});
}


// Whether a save holds Pure Heart number `heart` (0-7, the picture's position). A vanilla file shows
// the game's own way: by its story value against the heart's threshold.
static s32 heartShown(const spm::nandmgr::SaveFile * save, s32 heart, const u32 * threshold, f32 boxX, f32 boxY)
{
    // Once per file, with the first heart
    if (heart == 0 && game_mode::fileIsArchipelago(save))
        drawArchipelagoIcon(boxX, boxY);
    if (game_mode::fileIsVanilla(save))
        return static_cast<u32>(save->spmarioGlobals.gsw0) >= *threshold;
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
    sStub[2] = MR_R5_R21;
    sStub[3] = FMR_F1_F30;
    sStub[4] = FMR_F2_F29;
    code_patching::writeBranch(&sStub[5], reinterpret_cast<void *>(heartShown));
    code_patching::refreshCode(sStub, sizeof(sStub));

    code_patching::writeBranch(&at[0], sStub, true);
    at[1] = CMPWI_R3_0;
    at[2] = BEQ_SKIP;
    at[3] = NOP;
    code_patching::refreshCode(at, 4 * sizeof(u32));
    wii::os::OSReport("[spmap] filesel: Pure Hearts follow each chapter's progress\n");
}

}
