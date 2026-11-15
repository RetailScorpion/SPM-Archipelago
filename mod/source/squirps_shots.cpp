#include <common.h>
#include <msl/string.h>
#include <spm/mario.h>
#include <spm/spmario.h>
#include <spm/wpadmgr.h>
#include <wii/os.h>
#include <wii/wpad.h>

#include "code_patching.h"
#include "game_addresses.h"
#include "options_menu.h"
#include "squirps_shots.h"

namespace mod::squirps_shots {

#define SHOT_SIZE 0x30
#define VANILLA_SLOTS 0x18                 // 24
#define VANILLA_BLOCK 0x4fc
#define EXTRA_SHOTS 40
#define BLOCK_SIZE (VANILLA_BLOCK + EXTRA_SHOTS * SHOT_SIZE)
#define SLOT_COUNT (VANILLA_SLOTS + EXTRA_SHOTS)
#define EXTRA_FROM_INNER (0x6c + VANILLA_SLOTS * SHOT_SIZE)  // the extra slots, from the block's +0x10
#define NORMAL_BLOCK 0xd0                  // the normal shot block (4 slots after 0x10 bytes)
#define NORMAL_SLOTS 4
#define NORMAL_BIG (NORMAL_BLOCK + EXTRA_SHOTS * SHOT_SIZE)
#define NORMAL_COUNT (NORMAL_SLOTS + EXTRA_SHOTS)

// PowerPC encodings
#define LI(rd, imm) (0x38000000 | ((rd) << 21) | ((imm) & 0xFFFF))
#define CMPWI(ra, imm) (0x2C000000 | ((ra) << 16) | ((imm) & 0xFFFF))
#define ADDI(rd, ra, imm) (0x38000000 | ((rd) << 21) | ((ra) << 16) | ((imm) & 0xFFFF))
#define MR_R3_R23 0x7EE3BB78
#define BEQLR 0x4D820020
#define BLR 0x4E800020

static u32 * const ALLOC_SIZE = reinterpret_cast<u32 *>(0x80113628);   // li r4, 0x4fc (space mode)
static u32 * const MEMSET_SIZE = reinterpret_cast<u32 *>(0x8011363c);  // li r5, 4: memset(block, 0, 4)
static u32 * const COUNT_A = reinterpret_cast<u32 *>(0x80114c74);      // li r3, 0x18
static u32 * const COUNT_B = reinterpret_cast<u32 *>(0x80114f84);      // li r3, 0x18
static u32 * const CLEAR_LOOP = reinterpret_cast<u32 *>(0x80113540);   // cmpwi r29, 0x18
static u32 * const FIRE_SLOTS = reinterpret_cast<u32 *>(0x80113fc0);   // mr r3, r23: the gun's slots
static u32 * const FIRE_COUNT = reinterpret_cast<u32 *>(0x80113fcc);   // li r4, 4
// The normal block (Squirps in 4-1 uses it: user, 2026-11-09, the space block alone did nothing)
static u32 * const NORMAL_ALLOC = reinterpret_cast<u32 *>(0x8011361c);       // li r4, 0xd0
static u32 * const RESET_ALLOC = reinterpret_cast<u32 *>(0x80113568);        // li r4, 0xd0 (the block made again)
static u32 * const RESET_MEMSET = reinterpret_cast<u32 *>(0x80113578);       // li r5, 0xd0
static u32 * const NORMAL_COUNT_A = reinterpret_cast<u32 *>(0x80114c68);     // li r3, 4
static u32 * const NORMAL_COUNT_B = reinterpret_cast<u32 *>(0x80114f78);     // li r3, 4
static u32 * const NORMAL_FIRE_COUNT = reinterpret_cast<u32 *>(0x80113f48);  // li r4, 4: the fire routine's slots

// Zeroes the whole new block: r5 = its size (r29 says which block it is: 1 = space mode)
static u32 sMemsetStub[5];

static bool sReady;
static bool sOn;

static bool expect(u32 * at, u32 value)
{
    if (*at == value)
        return true;
    wii::os::OSReport("[spmap] squirps: unexpected %08x at %p, not patched\n", *at, at);
    return false;
}

void init()
{
    if (!expect(ALLOC_SIZE, LI(4, VANILLA_BLOCK)) || !expect(MEMSET_SIZE, LI(5, 4)) ||
        !expect(COUNT_A, LI(3, VANILLA_SLOTS)) || !expect(COUNT_B, LI(3, VANILLA_SLOTS)) ||
        !expect(CLEAR_LOOP, CMPWI(29, VANILLA_SLOTS)) || !expect(FIRE_SLOTS, MR_R3_R23) ||
        !expect(FIRE_COUNT, LI(4, 4)) || !expect(NORMAL_ALLOC, LI(4, NORMAL_BLOCK)) ||
        !expect(RESET_ALLOC, LI(4, NORMAL_BLOCK)) || !expect(RESET_MEMSET, LI(5, NORMAL_BLOCK)) ||
        !expect(NORMAL_COUNT_A, LI(3, NORMAL_SLOTS)) || !expect(NORMAL_COUNT_B, LI(3, NORMAL_SLOTS)) ||
        !expect(NORMAL_FIRE_COUNT, LI(4, NORMAL_SLOTS)))
        return;
    sMemsetStub[0] = CMPWI(29, 0);
    sMemsetStub[1] = LI(5, NORMAL_BIG);
    sMemsetStub[2] = BEQLR;
    sMemsetStub[3] = LI(5, BLOCK_SIZE);
    sMemsetStub[4] = BLR;
    code_patching::refreshCode(sMemsetStub, sizeof(sMemsetStub));
    code_patching::writeInstruction(ALLOC_SIZE, LI(4, BLOCK_SIZE));
    code_patching::writeBranch(MEMSET_SIZE, sMemsetStub, true);
    code_patching::writeInstruction(COUNT_A, LI(3, SLOT_COUNT));
    code_patching::writeInstruction(COUNT_B, LI(3, SLOT_COUNT));
    code_patching::writeInstruction(CLEAR_LOOP, CMPWI(29, SLOT_COUNT));
    code_patching::writeInstruction(NORMAL_ALLOC, LI(4, NORMAL_BIG));
    code_patching::writeInstruction(RESET_ALLOC, LI(4, NORMAL_BIG));
    code_patching::writeInstruction(RESET_MEMSET, LI(5, NORMAL_BIG));
    code_patching::writeInstruction(NORMAL_COUNT_A, LI(3, NORMAL_COUNT));
    code_patching::writeInstruction(NORMAL_COUNT_B, LI(3, NORMAL_COUNT));
    sReady = true;
    wii::os::OSReport("[spmap] squirps: room for %d more shots\n", EXTRA_SHOTS);
}

#define RAPID_FIRE_FRAMES 6  // a shot every 6 frames (10 a second) while 2 is held
// Where Squirps flies with Mario: 4-1 Outer Space and 4-3 (user, 2026-11-12)
static bool squirpsMap(const char * map)
{
    return msl::string::strncmp(map, "sp1_", 4) == 0 || msl::string::strncmp(map, "sp3_", 4) == 0;
}

// MarioWork miscFlags bit the game's Squirps fire code checks before firing (0x80113F2C)
#define SQUIRPS_FIRE_MODE 0x00200000

/*
    The game doesn't fire on a press: Mario's key code counts the frames 2 has been held (MarioWork
    held2Time, 0x320: 0 on the press, +1 a frame, -1 once let go; 0x80151724), and the fire check
    (0x801516A0, from Mario's motion at 0x801163A8) fires when that count is exactly 2. The count only
    restarts after a release, so the first version, which added presses to the Wii Remote's state, did
    nothing (user, 2026-11-13). Now, while 2 is held, the count is put back to 1 every RAPID_FIRE_FRAMES
    frames; the game's key code makes it 2 again in its frame, and fires.
*/
// The 2-held count is at MarioWork + 0x320 (0x80151724 writes it there; read from Dolphin's memory,
// 2026-11-14). spm-headers' MarioWork has it 2 bytes early (its padding before held1Time ends at 0x31C,
// so "held2Time" is really the 1-held count at 0x31E), which is why the second version did nothing.
#define HELD_2_TIME 0x320

void rapidFireTick()
{
    if (!options_menu::get(options_menu::OPTION_SQUIRPS_RAPID) || !squirpsMap(spm::spmario::gp->mapName))
        return;
    spm::mario::MarioWork * mario = spm::mario::marioGetPtr();
    if (!(mario->miscFlags & SQUIRPS_FIRE_MODE) || !(mario->buttonsHeld & WPAD_BTN_2) ||
        spm::mario::marioKeyOffChk())
        return;
    s16 & held2Time = *reinterpret_cast<s16 *>(reinterpret_cast<u8 *>(mario) + HELD_2_TIME);
    if (held2Time > RAPID_FIRE_FRAMES)
        held2Time = 1;
}

void tick()
{
    if (!sReady)
        return;
    bool on = options_menu::get(options_menu::OPTION_SQUIRPS_SHOTS);
    if (on == sOn)
        return;
    sOn = on;
    // r31 is the block's +0x10 there
    code_patching::writeInstruction(FIRE_SLOTS, on ? ADDI(3, 31, EXTRA_FROM_INNER) : MR_R3_R23);
    code_patching::writeInstruction(FIRE_COUNT, on ? LI(4, EXTRA_SHOTS) : LI(4, 4));
    // The normal block's shots follow each other, so its fire routine is just offered them all
    code_patching::writeInstruction(NORMAL_FIRE_COUNT, LI(4, on ? NORMAL_COUNT : NORMAL_SLOTS));
    wii::os::OSReport("[spmap] squirps: shot limit %s\n", on ? "removed" : "back");
}

}
