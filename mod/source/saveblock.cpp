#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/evt_msg.h>
#include <spm/evt_nand.h>
#include <spm/item_data.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "patch.h"
#include "saveblock.h"

namespace mod::saveblock {

#define SAVE_BLOCK_ICON 405  // after the AP icons (tools/wicon.py)
#define NAME_KEY "ap_save_block_name"
#define DESC_KEY "ap_save_block_desc"
#define CANT_USE_KEY "ap_save_block_cant_use"
#define PIT_MAP_PREFIX "dan_"  // both Pits of 100 Trials

// The 7-3 Peach prop: item 45's old look, moved to an unused pickup
#define PEACH_PROP_ITEM 11
#define PEACH_PROP_ICON 90
// The item argument of its evt_item_entry("momo", 45, ...) in an3_06 and an3_15 (relF section 5)
static const u32 sPeachPropArgs[] = {0x1026f0, 0x108d20};

// What the save block runs when hit (mobj_save_blk_sysevt), without its battery warning and
// first-save message
EVT_BEGIN(ap_save_block_evt)
    SET(LW(0), 0)
    RUN_CHILD_EVT(spm::evt_nand::evt_memcard_save)
    RETURN()
EVT_END()

// Using it where it's disabled, like the Return Pipe's "can't use it here"
EVT_BEGIN(ap_save_block_cant_use_evt)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(CANT_USE_KEY), 0, 0)
    RETURN()
EVT_END()

// Disabled in the current map (read by the menu stubs)
static u8 sDisabled;
// The script using it runs (read by the use stub)
static const spm::evtmgr::EvtScriptCode * sUseScript = ap_save_block_evt;

// Instruction encodings for the stubs
#define CMPWI_R25(imm) (0x2C190000 | (imm))
#define CMPWI_R0_0 0x2C000000
#define BNE_8 0x40820008
#define BEQ_8 0x41820008
#define LIS_R3(addr) (0x3C600000 | ((reinterpret_cast<u32>(addr) + 0x8000) >> 16))
#define LO(addr) (reinterpret_cast<u32>(addr) & 0xFFFF)
#define LWZ_R3_R3(addr) (0x80630000 | LO(addr))
#define LBZ_R0_R3(addr) (0x88030000 | LO(addr))

// keyitem_menu_use_check: cmplwi r0, 0x24; bne pipe. Offsets from it:
#define MENU_FLORO 0x8      // the Floro Sprout's case
#define MENU_RUN_EVT 0x2C   // bl evtEntryType (r3 = script, r4-r6 = 0), then hands the entry to the menu
#define MENU_PIPE 0x4C      // the Return Pipe's case, and every other key item
static const u32 CMPLWI_R0_FLORO = 0x28000024;
static const u32 BNE_PIPE = 0x40820048;

/*
    Replaces the bne: Floro Sprout as before, the Save Block runs ap_save_block_evt through the
    Floro Sprout's evtEntryType call, anything else goes on to the Return Pipe's check.
*/
static u32 sStub[12];

static void patchKeyItemMenu()
{
    u32 * at = gamesyms::keyitem_menu_use_check;
    if (at[0] != CMPLWI_R0_FLORO || at[1] != BNE_PIPE)
    {
        wii::os::OSReport("[spmap] saveblock: key item menu isn't what was expected, not patched\n");
        return;
    }
    sStub[0] = 0x28000000 | 0x24;                  // cmplwi r0, Floro Sprout
    sStub[1] = 0x40820008;                         // bne +8
    patch::writeBranch(&sStub[2], at + MENU_FLORO / 4);
    sStub[3] = 0x28000000 | SAVE_BLOCK_ITEM_ID;    // cmplwi r0, Save Block
    sStub[4] = 0x41820008;                         // beq +8
    patch::writeBranch(&sStub[5], at + MENU_PIPE / 4);
    sStub[6] = LIS_R3(&sUseScript);                // r3 = sUseScript
    sStub[7] = LWZ_R3_R3(&sUseScript);
    sStub[8] = 0x38800000;                         // li r4, 0
    sStub[9] = 0x38A00000;                         // li r5, 0
    sStub[10] = 0x38C00000;                        // li r6, 0
    patch::writeBranch(&sStub[11], at + MENU_RUN_EVT / 4);
    wii::os::DCFlushRange(sStub, sizeof(sStub));
    wii::os::ICInvalidateRange(sStub, sizeof(sStub));
    patch::writeBranch(&at[1], sStub);
}

/*
    The menu drawing's Return Pipe test (cmpwi r25, 0x32; bne normal), extended: the Return Pipe goes
    on to its own checks, the Save Block is drawn greyed out while sDisabled, anything else normally.
    Offsets from the cmpwi: normal (not greyed) and greyed drawing.
*/
struct GreyPatch
{
    u32 * at;
    u32 bne;     // the original bne to normal drawing
    u32 normal;
    u32 greyed;
    u32 stub[12];
};
static GreyPatch sGreyPatches[] = {
    {gamesyms::keyitem_icon_pipe_check, 0x4082005C, 0x60, 0x40, {}},
    {gamesyms::keyitem_name_pipe_check, 0x40820040, 0x44, 0x24, {}},
};
#define PIPE_ITEM 0x32

static void patchGreyOut(GreyPatch & p)
{
    if (p.at[0] != static_cast<u32>(CMPWI_R25(PIPE_ITEM)) || p.at[1] != p.bne)
    {
        wii::os::OSReport("[spmap] saveblock: key item drawing isn't what was expected, not patched\n");
        return;
    }
    u32 * s = p.stub;
    s[0] = CMPWI_R25(PIPE_ITEM);
    s[1] = BNE_8;
    patch::writeBranch(&s[2], p.at + 2);           // the Return Pipe's own checks
    s[3] = CMPWI_R25(SAVE_BLOCK_ITEM_ID);
    s[4] = BEQ_8;
    patch::writeBranch(&s[5], p.at + p.normal / 4);
    s[6] = LIS_R3(&sDisabled);
    s[7] = LBZ_R0_R3(&sDisabled);
    s[8] = CMPWI_R0_0;
    s[9] = BNE_8;
    patch::writeBranch(&s[10], p.at + p.normal / 4);
    patch::writeBranch(&s[11], p.at + p.greyed / 4);
    wii::os::DCFlushRange(s, sizeof(p.stub));
    wii::os::ICInvalidateRange(s, sizeof(p.stub));
    patch::writeBranch(p.at, s);
}

static void movePeachProp()
{
    for (u32 offset : sPeachPropArgs)
    {
        s32 * arg = reinterpret_cast<s32 *>(gamesyms::relF_section5) + offset / 4;
        if (*arg != SAVE_BLOCK_ITEM_ID)
        {
            wii::os::OSReport("[spmap] saveblock: 7-3 prop at %x isn't item 45, not patched\n", offset);
            continue;
        }
        *arg = PEACH_PROP_ITEM;
    }
    spm::item_data::itemDataTable[PEACH_PROP_ITEM].iconId = PEACH_PROP_ICON;
}

void init()
{
    spm::item_data::ItemData & item = spm::item_data::itemDataTable[SAVE_BLOCK_ITEM_ID];
    item.iconId = SAVE_BLOCK_ICON;
    item.nameMsg = NAME_KEY;
    item.descMsg = DESC_KEY;
    movePeachProp();
    patchKeyItemMenu();
    for (auto & p : sGreyPatches)
        patchGreyOut(p);
    wii::os::OSReport("[spmap] saveblock: item %d is the Save Block\n", SAVE_BLOCK_ITEM_ID);
}

void onMapChange(const char * nextMap)
{
    sDisabled = nextMap != nullptr && msl::string::strncmp(nextMap, PIT_MAP_PREFIX, sizeof(PIT_MAP_PREFIX) - 1) == 0;
    sUseScript = sDisabled ? ap_save_block_cant_use_evt : ap_save_block_evt;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, NAME_KEY) == 0)
        return "Save Block";
    if (msl::string::strcmp(msgName, DESC_KEY) == 0)
        return "Use it to save your game anywhere.";
    if (msl::string::strcmp(msgName, CANT_USE_KEY) == 0)
        return "You can't use this here.";
    return nullptr;
}

}
