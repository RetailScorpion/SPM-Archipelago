#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/evt_door.h>
#include <spm/evt_msg.h>
#include <spm/evt_nand.h>
#include <spm/item_data.h>
#include <spm/mario.h>
#include <spm/nandmgr.h>
#include <wii/os.h>

#include "code_patching.h"
#include "game_addresses.h"
#include "save_block.h"

namespace mod::save_block {

#define SAVE_BLOCK_ICON 406    // added by the patcher after the Archipelago icons (it skips 403)
#define NAME_KEY "ap_save_block_name"
#define DESC_KEY "ap_save_block_desc"
#define CANT_USE_KEY "ap_save_block_cant_use"
#define PIT_MAP_PREFIX "dan_"  // both Pits of 100 Trials

// The 7-3 Peach prop: item 45's old look, moved to an unused pickup
#define PEACH_PROP_ITEM 11
#define PEACH_PROP_ICON 90
// Where the item number (45) sits in its evt_item_entry("momo", 45, ...) calls in maps an3_06 and
// an3_15 (relF script section offsets)
static const u32 sPeachPropArgs[] = {0x1026f0, 0x108d20};

// What the save block runs when hit (mobj_save_blk_sysevt), without its battery warning and
// first-save message
EVT_BEGIN(ap_save_block_evt)
    SET(LW(0), 0)
    RUN_CHILD_EVT(spm::evt_nand::evt_memcard_save)
    RETURN()
EVT_END()

// Using it where it's disabled: "You can't use this here.", like the Return Pipe
EVT_BEGIN(ap_save_block_cant_use_evt)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(CANT_USE_KEY), 0, 0)
    RETURN()
EVT_END()

// Whether it's disabled in the current map (the menu stubs read this)
static u8 sDisabled;
// The script using it runs (the "use" stub reads this)
static const spm::evtmgr::EvtScriptCode * sUseScript = ap_save_block_evt;

// ---- PowerPC instructions for the stubs ----
#define CMPWI_R25(value) (0x2C190000 | (value))  // compare register 25 with a number
#define CMPLWI_R0(value) (0x28000000 | (value))  // compare register 0 with a number
#define CMPWI_R0_0 0x2C000000                     // compare register 0 with 0
#define BNE_8 0x40820008                          // if not equal, skip the next instruction
#define BEQ_8 0x41820008                          // if equal, skip the next instruction
#define LI_R4_0 0x38800000                        // register 4 = 0
#define LI_R5_0 0x38A00000                        // register 5 = 0
#define LI_R6_0 0x38C00000                        // register 6 = 0
// Loading a value from the mod's memory takes two instructions: the high half of the address,
// then a load using the low half
#define LIS_R3(addr) (0x3C600000 | ((reinterpret_cast<u32>(addr) + 0x8000) >> 16))
#define LOW_HALF(addr) (reinterpret_cast<u32>(addr) & 0xFFFF)
#define LWZ_R3_R3(addr) (0x80630000 | LOW_HALF(addr))  // load 4 bytes into register 3
#define LBZ_R0_R3(addr) (0x88030000 | LOW_HALF(addr))  // load 1 byte into register 0

// ---- Using it from the key item menu ----

#define ITEM_FLORO_SPROUT 0x24
// At keyitem_menu_use_check: "cmplwi r0, Floro Sprout" then "bne" to the Return Pipe's check
static const u32 CMPLWI_R0_FLORO = 0x28000024;
static const u32 BNE_PIPE = 0x40820048;
// Places after that check (bytes from it)
#define MENU_FLORO 0x8      // the Floro Sprout's case
#define MENU_RUN_EVT 0x2C   // where it runs a script: evtEntryType (r3 = script, r4-r6 = 0), then
                            // hands the running script to the menu
#define MENU_PIPE 0x4C      // the Return Pipe's case, and every other key item

static u32 sUseStub[12];

/*
    Replaces the menu's "bne" with a jump to this stub:
      - Floro Sprout: its case, as before
      - Save Block: runs sUseScript, through the Floro Sprout's script-running code
      - anything else: on to the Return Pipe's check, as before
*/
static void patchKeyItemMenu()
{
    u32 * at = game_addresses::keyitem_menu_use_check;
    if (at[0] != CMPLWI_R0_FLORO || at[1] != BNE_PIPE)
    {
        wii::os::OSReport("[spmap] saveblock: key item menu isn't what was expected, not patched\n");
        return;
    }
    sUseStub[0] = CMPLWI_R0(ITEM_FLORO_SPROUT);  // Floro Sprout?
    sUseStub[1] = BNE_8;
    code_patching::writeBranch(&sUseStub[2], at + MENU_FLORO / 4);  //   yes: its case
    sUseStub[3] = CMPLWI_R0(SAVE_BLOCK_ITEM_ID);  // Save Block?
    sUseStub[4] = BEQ_8;
    code_patching::writeBranch(&sUseStub[5], at + MENU_PIPE / 4);   //   no: the Return Pipe's check
    sUseStub[6] = LIS_R3(&sUseScript);            //   yes: r3 = sUseScript
    sUseStub[7] = LWZ_R3_R3(&sUseScript);
    sUseStub[8] = LI_R4_0;
    sUseStub[9] = LI_R5_0;
    sUseStub[10] = LI_R6_0;
    code_patching::writeBranch(&sUseStub[11], at + MENU_RUN_EVT / 4);  // run it
    code_patching::refreshCode(sUseStub, sizeof(sUseStub));
    code_patching::writeBranch(&at[1], sUseStub);
}

// ---- Greying it out in the key item menu ----

#define ITEM_RETURN_PIPE 0x32

/*
    The menu draws each key item's picture and name. Before each, it tests "is this the Return Pipe"
    (cmpwi r25, 0x32; bne normal) to decide whether to grey it out. Both tests are extended:
      - Return Pipe: on to its own checks, as before
      - Save Block: greyed out while sDisabled is set, otherwise normal
      - anything else: normal
*/
struct GreyOutPatch
{
    u32 * at;
    u32 bne;     // the game's original "bne normal"
    u32 normal;  // bytes from the test to the normal drawing
    u32 greyed;  // bytes from the test to the greyed-out drawing
    u32 stub[12];
};
static GreyOutPatch sGreyOutPatches[] = {
    {game_addresses::keyitem_icon_pipe_check, 0x4082005C, 0x60, 0x40, {}},  // the picture
    {game_addresses::keyitem_name_pipe_check, 0x40820040, 0x44, 0x24, {}},  // the name
};

static void patchGreyOut(GreyOutPatch & p)
{
    if (p.at[0] != static_cast<u32>(CMPWI_R25(ITEM_RETURN_PIPE)) || p.at[1] != p.bne)
    {
        wii::os::OSReport("[spmap] saveblock: key item drawing isn't what was expected, not patched\n");
        return;
    }
    u32 * s = p.stub;
    s[0] = CMPWI_R25(ITEM_RETURN_PIPE);       // Return Pipe?
    s[1] = BNE_8;
    code_patching::writeBranch(&s[2], p.at + 2);              //   yes: its own checks
    s[3] = CMPWI_R25(SAVE_BLOCK_ITEM_ID);     // Save Block?
    s[4] = BEQ_8;
    code_patching::writeBranch(&s[5], p.at + p.normal / 4);   //   no: normal
    s[6] = LIS_R3(&sDisabled);                //   yes: disabled here?
    s[7] = LBZ_R0_R3(&sDisabled);
    s[8] = CMPWI_R0_0;
    s[9] = BNE_8;
    code_patching::writeBranch(&s[10], p.at + p.normal / 4);  //     no: normal
    code_patching::writeBranch(&s[11], p.at + p.greyed / 4);  //     yes: greyed out
    code_patching::refreshCode(s, sizeof(p.stub));
    code_patching::writeBranch(p.at, s);
}

// ---- Saving inside a room ----

/*
    Houses and other rooms entered by a door are part of their map, but a loaded save always starts
    with every room shut, so a save made inside one would put Mario where the room was, behind its
    closed walls (found in Saffron's house). So a save made in a room records the last spot Mario
    stood outside one, in front of the door he went in by, and loading puts him there.
*/
static wii::mtx::Vec3 sOutside;
static bool sHaveOutside;

// Whether Mario is in a room, or going through a room's door.
static bool inRoom()
{
    return spm::evt_door::evtDoorGetActiveDoorDesc() != nullptr ||
           (spm::evt_door::evt_door_wp->flags & EVT_DOOR_FLAG_DOOR_OPEN) != 0;
}

// Saves the game (every save goes through here); saved inside a room, with Mario outside it.
static void (*nandUpdateSaveReal)(s32 saveId);
static void nandUpdateSaveHook(s32 saveId)
{
    spm::mario::MarioWork * mario = spm::mario::marioGetPtr();
    if (!inRoom() || !sHaveOutside)
    {
        nandUpdateSaveReal(saveId);
        return;
    }
    wii::mtx::Vec3 inside = mario->position;
    mario->position = sOutside;  // only for the save: nothing runs before he's put back
    nandUpdateSaveReal(saveId);
    mario->position = inside;
    wii::os::OSReport("[spmap] saveblock: saved in a room, at the spot outside it\n");
}

// ---- Setup ----

// Moves the 7-3 Peach prop from item 45 to item 11, with item 45's old picture.
static void movePeachProp()
{
    for (u32 offset : sPeachPropArgs)
    {
        s32 * arg = reinterpret_cast<s32 *>(game_addresses::relF_section5) + offset / 4;
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
    for (auto & p : sGreyOutPatches)
        patchGreyOut(p);
    nandUpdateSaveReal = code_patching::hookFunction(spm::nandmgr::nandUpdateSave, nandUpdateSaveHook);
    wii::os::OSReport("[spmap] saveblock: item %d is the Save Block\n", SAVE_BLOCK_ITEM_ID);
}

void onMapChange(const char * nextMap)
{
    sDisabled = nextMap != nullptr && msl::string::strncmp(nextMap, PIT_MAP_PREFIX, sizeof(PIT_MAP_PREFIX) - 1) == 0;
    sUseScript = sDisabled ? ap_save_block_cant_use_evt : ap_save_block_evt;
    sHaveOutside = false;
}

void tick()
{
    if (inRoom())
        return;
    sOutside = spm::mario::marioGetPtr()->position;
    sHaveOutside = true;
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
